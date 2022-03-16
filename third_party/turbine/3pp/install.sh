#!/bin/bash
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
DEPS_PREFIX="$2"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
MAVEN_VERSION="3.8.5"

curl -O https://downloads.apache.org/maven/maven-3/$MAVEN_VERSION/binaries/apache-maven-$MAVEN_VERSION-bin.tar.gz
tar xzf apache-maven-$MAVEN_VERSION-bin.tar.gz
mv apache-maven-$MAVEN_VERSION $SCRIPT_DIR/

# Verify mvn works
JAVA_HOME=$DEPS_PREFIX/current PATH=$SCRIPT_DIR/apache-maven-$MAVEN_VERSION/bin:$PATH mvn -v

# Build
JAVA_HOME=$DEPS_PREFIX/current PATH=$SCRIPT_DIR/apache-maven-$MAVEN_VERSION/bin:$PATH mvn package -DskipTests=true -q -f pom.xml
cp target/turbine-HEAD-SNAPSHOT-all-deps.jar turbine.jar

mkdir -p $PREFIX/
cp turbine.jar $PREFIX/
