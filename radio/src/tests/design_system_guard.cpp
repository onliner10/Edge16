/*
 * Copyright (C) EdgeTX
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// Shells out to tools/design-system/check_design_system.py (see
// DESIGN_SYSTEM.md) so a design-system regression fails gtests-radio, not
// just a standalone CI step. The script itself prints file:line for every
// violation over baseline; that output is echoed into the gtest failure so a
// developer sees exactly what to fix without re-running anything.

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <string>

#include "location.h"

namespace {

// TESTS_PATH is "<repo_root>/radio/src/tests" (see location.h.in); walk back
// up to the repo root rather than trusting the process CWD.
std::string designSystemScriptPath()
{
  return std::string(TESTS_PATH) +
         "/../../../tools/design-system/check_design_system.py";
}

// Runs `cmd`, returns its exit status (as from WEXITSTATUS) and captures
// combined stdout+stderr into `output`.
int runCapture(const std::string& cmd, std::string& output)
{
  std::string full = cmd + " 2>&1";
  FILE* pipe = popen(full.c_str(), "r");
  if (!pipe) return -1;

  std::array<char, 4096> buf{};
  size_t n;
  while ((n = fread(buf.data(), 1, buf.size(), pipe)) > 0) {
    output.append(buf.data(), n);
  }
  int status = pclose(pipe);
  if (status == -1) return -1;
#if defined(WIFEXITED) && defined(WEXITSTATUS)
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
#else
  return status;
#endif
}

}  // namespace

// See DESIGN_SYSTEM.md §6 "Enforcement" — the CI guard runs as a gtest so a
// design-system regression fails the build, not just a review comment.
TEST(DesignSystem, GuardClean)
{
  std::string script = designSystemScriptPath();
  std::string cmd = "python3 \"" + script + "\"";

  std::string output;
  int exitCode = runCapture(cmd, output);

  EXPECT_EQ(0, exitCode)
      << "Design-system guard failed (tools/design-system/"
         "check_design_system.py). Fix the reported raw-styling violations "
         "or, if a file's count genuinely dropped, rerun with "
         "--update-baseline and commit the lowered baseline.\n\n"
      << output;
}
