#!/bin/bash

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

echo "*************************************************"
echo "* Do not source cr.sh ***************************"
echo "*************************************************"
echo "* You need to source cr-bash-helpers.sh instead *"
echo "* This file will stop working and be removed    *"
echo "* soon.                                         *"
echo "*************************************************"
source $(dirname $(realpath "${BASH_SOURCE:-$0}"))/cr-bash-helpers.sh
