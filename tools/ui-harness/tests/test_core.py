"""Unit tests for tools/ui-harness/edgetx_ui/core.py.

Run with:
    cd tools/ui-harness && python3 -m unittest discover -s tests -v

No simulator process is spawned; SdlAutomationSession.command() is exercised
directly against a mocked subprocess.Popen so these tests run anywhere,
including outside the Nix dev shell.
"""

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from edgetx_ui.core import (
    HarnessError,
    SdlAutomationSession,
    _ccache_launcher_flags,
    _check_radio_yaml_checksum,
    _crc16_ccitt,
    _prepare_settings_directory,
    _provision_default_radio_yaml,
    _radio_yaml_declared_and_computed_checksum,
    normalize_yaml_line_endings,
)


def _make_session() -> SdlAutomationSession:
    """A minimal SdlAutomationSession with a fake running process, bypassing
    __init__ (which would touch the filesystem for fixtures/tempdirs)."""
    session = SdlAutomationSession.__new__(SdlAutomationSession)
    session.process = MagicMock()
    session.process.poll.return_value = None  # still running
    session.process.stdin = MagicMock()
    session._seq = 0
    session._last_read_tail = ""
    return session


class CommandRetryTests(unittest.TestCase):
    """Covers P2: command() retries a bare timeout for read-only queries but
    never retries by default, so mutating commands are not silently resent."""

    def test_retries_on_timeout_then_succeeds(self):
        session = _make_session()
        # First two reads time out (simulate the simulator being mid
        # storage-flush and missing its reply window); the third succeeds.
        responses = [None, None, {"ok": True, "seq": 99, "ui": {"nodes": []}}]
        session._read_response = MagicMock(side_effect=responses)

        result = session.command("ui_tree", timeout=1.0, retries=2, retry_backoff=0)

        self.assertEqual(result, {"ok": True, "seq": 99, "ui": {"nodes": []}})
        self.assertEqual(session._read_response.call_count, 3)

    def test_raises_clear_error_after_retries_exhausted(self):
        session = _make_session()
        session._read_response = MagicMock(return_value=None)

        with self.assertRaises(HarnessError) as ctx:
            session.command("ui_tree", timeout=1.0, retries=2, retry_backoff=0)

        # Exhausts exactly retries+1 attempts, and the error is unambiguous.
        self.assertEqual(session._read_response.call_count, 3)
        self.assertIn("timed out waiting for simulator response to `ui_tree`", str(ctx.exception))

    def test_default_is_no_retry(self):
        """Mutating commands must not be retried by default: resending on a
        timeout could double-apply an action if the original request landed
        and only the reply was delayed."""
        session = _make_session()
        session._read_response = MagicMock(return_value=None)

        with self.assertRaises(HarnessError):
            session.command("press ENTER 120", timeout=1.0)

        self.assertEqual(session._read_response.call_count, 1)

    def test_error_reply_is_not_retried(self):
        """retries only applies to a bare timeout (no reply observed); an
        actual error reply from the simulator must surface immediately."""
        session = _make_session()
        session._read_response = MagicMock(return_value={"ok": False, "error": "boom", "seq": 1})

        with self.assertRaises(HarnessError) as ctx:
            session.command("ui_tree", timeout=1.0, retries=2, retry_backoff=0)

        self.assertEqual(session._read_response.call_count, 1)
        self.assertIn("boom", str(ctx.exception))

    def test_command_not_running_raises_before_any_send(self):
        session = _make_session()
        session.process.poll.return_value = 1  # exited
        with self.assertRaises(HarnessError):
            session.command("ui_tree", retries=2)


class CcacheLauncherFlagsTests(unittest.TestCase):
    """Covers P3: ccache launcher flags are opt-out, conditional on PATH."""

    def test_absent_when_ccache_not_on_path(self):
        import edgetx_ui.core as core

        original_which = core.shutil.which
        core.shutil.which = lambda name: None if name == "ccache" else original_which(name)
        try:
            self.assertEqual(_ccache_launcher_flags(), [])
        finally:
            core.shutil.which = original_which

    def test_present_when_ccache_on_path(self):
        import edgetx_ui.core as core

        original_which = core.shutil.which
        core.shutil.which = lambda name: "/usr/bin/ccache" if name == "ccache" else original_which(name)
        try:
            flags = _ccache_launcher_flags()
            self.assertIn("-DCMAKE_C_COMPILER_LAUNCHER=ccache", flags)
            self.assertIn("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache", flags)
        finally:
            core.shutil.which = original_which

    def test_opt_out_env_var_disables_even_when_present(self):
        import edgetx_ui.core as core

        original_which = core.shutil.which
        core.shutil.which = lambda name: "/usr/bin/ccache" if name == "ccache" else original_which(name)
        import os

        old = os.environ.get("EDGETX_UI_NO_CCACHE")
        os.environ["EDGETX_UI_NO_CCACHE"] = "1"
        try:
            self.assertEqual(_ccache_launcher_flags(), [])
        finally:
            core.shutil.which = original_which
            if old is None:
                os.environ.pop("EDGETX_UI_NO_CCACHE", None)
            else:
                os.environ["EDGETX_UI_NO_CCACHE"] = old


def _radio_yaml_body(checksum: int | None, extra: str = "vBatWarn: 66\r\n") -> bytes:
    """A minimal (not firmware-schema-complete, but checksum-shaped) radio.yml
    body: a leading checksum line (or none) followed by CRLF content."""
    prefix = f"checksum: {checksum}\r\n" if checksum is not None else ""
    return (prefix + extra).encode()


class RadioYamlChecksumTests(unittest.TestCase):
    """Covers P6a: the harness replicates readYamlFile()'s checksum check
    (radio/src/storage/sdcard_yaml.cpp) in Python so a bad fixture fails
    fast with an actionable error instead of a cryptic firmware trace."""

    def test_crc16_matches_known_test_vector(self):
        # Standard CRC-16/CCITT-FALSE("123456789") == 0x29B1.
        self.assertEqual(_crc16_ccitt(b"123456789"), 0x29B1)

    def test_declared_and_computed_agree_for_a_correctly_stamped_file(self):
        body = b"vBatWarn: 66\r\nvBatCrit: 63\r\n"
        checksum = _crc16_ccitt(body)
        data = f"checksum: {checksum}\r\n".encode() + body
        declared, computed = _radio_yaml_declared_and_computed_checksum(data)
        self.assertEqual(declared, checksum)
        self.assertEqual(computed, checksum)

    def test_no_checksum_line_is_not_an_error(self):
        # Matches readYamlFile(): a file with no leading "checksum: " line
        # is treated as legacy and the check is skipped outright.
        data = b"vBatWarn: 66\r\nvBatCrit: 63\r\n"
        declared, computed = _radio_yaml_declared_and_computed_checksum(data)
        self.assertIsNone(declared)

    def test_check_passes_for_correctly_stamped_file(self):
        body = b"vBatWarn: 66\r\n"
        checksum = _crc16_ccitt(body)
        data = f"checksum: {checksum}\r\n".encode() + body
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "radio.yml"
            path.write_bytes(data)
            _check_radio_yaml_checksum(path)  # must not raise

    def test_check_raises_actionable_error_for_mismatch(self):
        body = b"vBatWarn: 66\r\n"
        wrong_checksum = (_crc16_ccitt(body) + 1) & 0xFFFF
        data = f"checksum: {wrong_checksum}\r\n".encode() + body
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "radio.yml"
            path.write_bytes(data)
            with self.assertRaises(HarnessError) as ctx:
                _check_radio_yaml_checksum(path)
            message = str(ctx.exception)
            self.assertIn(str(wrong_checksum), message)
            self.assertIn(str(path), message)
            self.assertIn("manuallyEdited", message)
            # A checksum-rejected boot doesn't just fail to start: the
            # firmware's own recovery path reformats the settings overlay
            # in place (storageEraseAll -> storageFormat), silently
            # clobbering MODELS/*.yml with a fresh default model. The
            # error must say so, not just "invalid checksum".
            self.assertIn("storageEraseAll", message)
            self.assertIn("overwrit", message)

    def test_manually_edited_forgives_a_checksum_mismatch(self):
        # Matches the firmware's own bypass: a successfully-parsed file
        # with manuallyEdited: 1 is accepted despite a checksum mismatch.
        body = b"vBatWarn: 66\r\nmanuallyEdited: 1\r\n"
        wrong_checksum = (_crc16_ccitt(body) + 1) & 0xFFFF
        data = f"checksum: {wrong_checksum}\r\n".encode() + body
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "radio.yml"
            path.write_bytes(data)
            _check_radio_yaml_checksum(path)  # must not raise

    def test_empty_file_raises_actionable_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "radio.yml"
            path.write_bytes(b"")
            with self.assertRaises(HarnessError):
                _check_radio_yaml_checksum(path)


class SettingsFixtureValidationTests(unittest.TestCase):
    """Covers P6: normalization/validation of a settings-overlay directory
    runs the same way regardless of whether it is a copied fixture or an
    explicit --settings path (the previous gap: explicit paths skipped
    normalize_yaml_line_endings entirely, so real-device-style CRLF
    checksums silently mismatched after an LF-saving edit)."""

    def test_prepare_settings_directory_normalizes_line_endings(self):
        body = b"vBatWarn: 66\n"  # LF, as most editors/git would save it
        checksum = _crc16_ccitt(body.replace(b"\n", b"\r\n"))  # stamped for CRLF, like a real export
        data = f"checksum: {checksum}\n".encode() + body
        with tempfile.TemporaryDirectory() as tmp:
            settings = Path(tmp) / "settings"
            (settings / "RADIO").mkdir(parents=True)
            (settings / "RADIO" / "radio.yml").write_bytes(data)
            # Before normalization this would fail: the checksum was
            # stamped assuming CRLF bytes but the file on disk is LF-only.
            _prepare_settings_directory(settings, "tx16s")  # must not raise
            normalized = (settings / "RADIO" / "radio.yml").read_bytes()
            self.assertIn(b"\r\n", normalized)

    def test_prepare_settings_directory_provisions_missing_radio_yaml(self):
        with tempfile.TemporaryDirectory() as tmp:
            settings = Path(tmp) / "settings"
            (settings / "MODELS").mkdir(parents=True)
            (settings / "MODELS" / "model1.yml").write_text("name: Test\n")
            self.assertFalse((settings / "RADIO" / "radio.yml").exists())
            _prepare_settings_directory(settings, "tx16s")
            provisioned = settings / "RADIO" / "radio.yml"
            self.assertTrue(provisioned.exists())
            # The provisioned file must itself pass the checksum check.
            _check_radio_yaml_checksum(provisioned)

    def test_prepare_settings_directory_leaves_models_only_settings_alone_for_unknown_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            settings = Path(tmp) / "settings"
            (settings / "MODELS").mkdir(parents=True)
            (settings / "MODELS" / "model1.yml").write_text("name: Test\n")
            # No default fixture exists for this made-up target name, so
            # provisioning is a no-op rather than an error: the firmware's
            # own first-run defaulting takes over, matching prior behavior.
            _prepare_settings_directory(settings, "not-a-real-target")
            self.assertFalse((settings / "RADIO" / "radio.yml").exists())

    def test_prepare_settings_directory_validates_existing_radio_yaml(self):
        body = b"vBatWarn: 66\r\n"
        wrong_checksum = (_crc16_ccitt(body) + 1) & 0xFFFF
        data = f"checksum: {wrong_checksum}\r\n".encode() + body
        with tempfile.TemporaryDirectory() as tmp:
            settings = Path(tmp) / "settings"
            (settings / "RADIO").mkdir(parents=True)
            (settings / "RADIO" / "radio.yml").write_bytes(data)
            with self.assertRaises(HarnessError):
                _prepare_settings_directory(settings, "tx16s")

    def test_prepare_settings_directory_no_op_for_missing_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            missing = Path(tmp) / "does-not-exist"
            _prepare_settings_directory(missing, "tx16s")  # must not raise


if __name__ == "__main__":
    unittest.main()
