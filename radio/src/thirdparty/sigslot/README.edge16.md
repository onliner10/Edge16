<!--
SPDX-License-Identifier: GPL-2.0-only
-->

# sigslot for Edge16

This directory vendors the header-only `palacaze/sigslot` library for the
color LCD UI event layer.

- Upstream: https://github.com/palacaze/sigslot
- Version: v1.2.3
- Commit: b588b791b9cf7eb17ff0a74d8aebd4a61166c2e1
- License: MIT, copied in `LICENSE`

Edge16 code should not include `sigslot/signal.hpp` directly. Use the
wrappers in `radio/src/gui/colorlcd/libui/ui_signal.h` so UI subscriptions are
owned by explicit Edge16 lifetimes.

## Local patch

`ui_signal.h` defines `SIGSLOT_EDGE16_NO_STD_MUTEX_SIGNAL` before including
the upstream header. This disables the unused thread-safe `sigslot::signal` and
`sigslot::observer` aliases because the firmware toolchain may not provide
`std::mutex`. Edge16 uses `sigslot::signal_st` in the single UI thread instead.
