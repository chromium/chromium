#!/bin/bash

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run this script to fetch the latest CLDR files from unicode.org.

# WARNING: This will remove all existing files in //third_party/cldr/src.

# Currently only fetches files needed for emoji keywords in English.
# If needed, update the unzip line as appropriate.

# CLDR release to checkout. See http://cldr.unicode.org/index/downloads
CLDR_URL='http://unicode.org/Public/cldr/44/cldr-common-44.0.zip'
# To update the CLDR files, change this URL and also update the Version
# field in README.chromium. Then run this script and commit the changes.

# Set working directory and terminate on error.
set -e
cd "$(dirname "$0")"

# Download release zip.
curl "$CLDR_URL" -o cldr.zip

# Remove existing src directory.
rm -rf src

# Unzip relevant files into src directory and clean zip.
unzip -d src -o cldr.zip common/annotations{Derived,}/{en,en_001,da,de,es,fi,fr,ja,no,sv}.xml \
                         common/supplemental/plurals.xml
rm -v cldr.zip
