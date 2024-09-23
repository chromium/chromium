#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
DEPS_PREFIX="$2"

# Verify mvn works
JAVA_HOME=$DEPS_PREFIX mvn -v

# Build
JAVA_HOME=$DEPS_PREFIX mvn package -DskipTests=true -q -f pom.xml
cp target/turbine-HEAD-SNAPSHOT-all-deps.jar turbine.jar

mkdir -p $PREFIX/
cp turbine.jar $PREFIX/
