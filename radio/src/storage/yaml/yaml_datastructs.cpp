/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
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

#include "edgetx.h"
#include "yaml_node.h"

// Warning: this file must be kept in sync with the CMakeLists.txt
//          in the same directory.

#include "yaml_inputs.inc"
#include "yaml_datastructs_funcs.cpp"

#if defined(PCBX10) && defined(PCBREV_TX16S)
 #include "yaml_datastructs_x10.cpp"
#elif defined(PCBTX16SMK3)
 #include "yaml_datastructs_tx16smk3.cpp"
#else
#error "Only TX16S MK2 and TX16S MK3 are supported by Edge16 YAML storage"
#endif
