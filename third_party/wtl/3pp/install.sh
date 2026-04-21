#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

PREFIX="$1"
TARGET_FILE="lib/native/include/atlapp.h"

# Remove " != FALSE" from `SystemParametersInfo` calls inside `ATLVERIFY`.
SPI_STR='SystemParametersInfo(SPI_GETICONTITLELOGFONT, '
SPI_STR+='sizeof(LOGFONT), \&lf, 0)'

sed -i "s/${SPI_STR} != FALSE/${SPI_STR}/g" "$TARGET_FILE"

# Wrap the `GetObject` `ATLVERIFY` in a (void) cast.
OBJ_MATCH='ATLVERIFY(::GetObject(hFont, sizeof(LOGFONT), \&lf) == '
OBJ_MATCH+='sizeof(LOGFONT))'

sed -i "s/${OBJ_MATCH}/ (void)(${OBJ_MATCH})/g" "$TARGET_FILE"

# Finally, copy the files.
mkdir -p "$PREFIX/include"
cp -r lib/native/include/* "$PREFIX/include/"