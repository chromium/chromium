#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# The leaf filename of the downloaded package, sans extension.
PACKAGE_NAME="$1"

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# So the commands below should output the built product to this directory.
PREFIX="$2"

mv raw_source_0.tar.gz $PACKAGE_NAME.pkg
pkgutil --expand-full $PACKAGE_NAME.pkg output
cp -R output/$PACKAGE_NAME-package.pkg/Payload/* "$PREFIX"
