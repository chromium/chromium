#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

# `PREFIX` is the destination directory for the CIPD package content.
PREFIX="$1"

# NuGet structure is:
#   build/native/include/WebView2.h
#   build/native/{x86,x64,arm64}/...
mkdir -p "$PREFIX/include"
if [ -f "build/native/include/WebView2.h" ]; then
    cp "build/native/include/WebView2.h" "$PREFIX/include/"
else
    echo "Error: WebView2.h not found in expected location."
    exit 1
fi

# Copy Architecture-specific Binaries.
for arch in x86 x64 arm64
do
    SRC_DIR="build/native/$arch"
    DST_DIR="$PREFIX/$arch"

    if [ -d "$SRC_DIR" ]; then
        mkdir -p "$DST_DIR"

        # Copy the DLL.
        if [ -f "$SRC_DIR/WebView2Loader.dll" ]; then
            cp "$SRC_DIR/WebView2Loader.dll" "$DST_DIR/"
        else
            echo "Error: WebView2Loader.dll not found in expected location."
            exit 1
        fi

        # Copy the Import Library (.dll.lib).
        if [ -f "$SRC_DIR/WebView2Loader.dll.lib" ]; then
            cp "$SRC_DIR/WebView2Loader.dll.lib" "$DST_DIR/"
        else
            echo "Error: WebView2Loader.dll.lib not found in expected location."
            exit 1
        fi
    else
        echo "Error: Directory for $arch not found."
        exit 1
    fi
done
