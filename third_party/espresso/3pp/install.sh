#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
DEPS_PREFIX="$2"

mkdir "$PREFIX/lib/"

export JAVA_HOME=$DEPS_PREFIX/current

JETIFIER_CONFIG="./.3pp/chromium/third_party/espresso/jetifier.config"

$DEPS_PREFIX/bin/jetifier-standalone -c $JETIFIER_CONFIG -i ./lib/espresso-intents-release-no-dep.jar -o $PREFIX/lib/espresso-intents-release-no-dep.jar
$DEPS_PREFIX/bin/jetifier-standalone -c $JETIFIER_CONFIG -i ./lib/espresso-web-release-no-dep.jar -o $PREFIX/lib/espresso-web-release-no-dep.jar
$DEPS_PREFIX/bin/jetifier-standalone -c $JETIFIER_CONFIG -i ./lib/espresso-core-release-no-dep.jar -o $PREFIX/lib/espresso-core-release-no-dep.jar
$DEPS_PREFIX/bin/jetifier-standalone -c $JETIFIER_CONFIG -i ./lib/espresso-idling-resource-release-no-dep.jar -o $PREFIX/lib/espresso-idling-resource-release-no-dep.jar
$DEPS_PREFIX/bin/jetifier-standalone -c $JETIFIER_CONFIG -i ./lib/espresso-contrib-release-no-dep.jar -o $PREFIX/lib/espresso-contrib-release-no-dep.jar
