#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

git diff --relative=third_party/federated_compute/third_party/googleapis \
  --src-prefix=a/ --dst-prefix=b/ \
  -- "${SCRIPT_DIR}/../src" > "${SCRIPT_DIR}/../patches/999-new_patch.patch"
