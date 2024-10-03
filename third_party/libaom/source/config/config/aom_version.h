/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_VERSION_H_
#define AOM_VERSION_H_
#define VERSION_MAJOR 3
#define VERSION_MINOR 10
#define VERSION_PATCH 0
#define VERSION_EXTRA "120-g669fcf17a7"
#define VERSION_PACKED \
  ((VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | (VERSION_PATCH))
#define VERSION_STRING_NOSP "3.10.0-120-g669fcf17a7"
#define VERSION_STRING " 3.10.0-120-g669fcf17a7"
#endif  // AOM_VERSION_H_
