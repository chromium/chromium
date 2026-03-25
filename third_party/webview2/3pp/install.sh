#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

# `PREFIX` is the destination directory for the CIPD package content.
PREFIX="$1"

# Exit if a destination was not provided.
if [[ -z "$PREFIX" ]]; then
    echo "Usage: $0 <destination_directory>"
    exit 1
fi

# NuGet structure is:
#   build/native/include/WebView2.h
#   build/native/{x86,x64,arm64}/...
mkdir -p "$PREFIX/include"

if [[ -f "build/native/include/WebView2.h" ]]; then
    cp "build/native/include/WebView2.h" "$PREFIX/include/"
else
    echo "Error: WebView2.h not found in expected location."
    exit 1
fi

# Copy Architecture-specific Binaries.
for arch in x86 x64 arm64; do
    SRC_DIR="build/native/$arch"
    DST_DIR="$PREFIX/$arch"

    # Exit early if the source directory does not exist.
    if [[ ! -d "$SRC_DIR" ]]; then
        echo "Error: Directory for $arch not found at $SRC_DIR."
        exit 1
    fi

    mkdir -p "$DST_DIR"

    # Copy the DLL, Import Library (.dll.lib), and Static Library.
    FILES=(
        "WebView2Loader.dll"
        "WebView2Loader.dll.lib"
        "WebView2LoaderStatic.lib"
    )

    for file in "${FILES[@]}"; do
        if [[ -f "$SRC_DIR/$file" ]]; then
            cp "$SRC_DIR/$file" "$DST_DIR/"
        else
            echo "Error: $file not found in $SRC_DIR."
            exit 1
        fi
    done
done