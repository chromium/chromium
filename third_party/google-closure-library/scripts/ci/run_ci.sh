#!/bin/sh

# Copyright The Closure Library Authors.
# SPDX-License-Identifier: Apache-2.0

set -ex

cd "${KOKORO_ARTIFACTS_DIR}/git/closure-library-staging"

# Install Node 14.

tar xf "${KOKORO_GFILE_DIR}/node-${NODE_VERSION}-linux-x64.tar.xz"
export PATH
PATH="$(pwd)/node-${NODE_VERSION}-linux-x64/bin:${PATH}"

# Generate docs (without pushing them anywhere).

export GH_PAGES
GH_PAGES=$(mktemp -d)

# TODO(user): Doc generation temporarily disabled due to Dossier issues.
# git clone --depth=1 https://github.com/google/closure-library "$GH_PAGES"
# ./scripts/ci/generate_latest_docs.sh

# Compile Closure Library with the current latest compiler release.

./scripts/ci/compile_closure.sh "${KOKORO_ARTIFACTS_DIR}/${LATEST_COMPILER_JAR_PATH}"

# Compile Closure Library with the nightly external compiler.

./scripts/ci/install_closure_deps.sh
./scripts/ci/compile_closure.sh ../closure-compiler-1.0-SNAPSHOT.jar

# Ensure that all generated files are generated without error.

npm install
npm run check_closure_oss
npm run gen_deps_js
npm run gen_deps_js_with_tests
npm run gen_test_htmls
npm run gen_alltests_js

# Run tests for closure-deps.

cd closure-deps
npm install
npm test
