#!/bin/bash
#
# Copyright 2018 The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Updates the gh-pages branch in $GH_PAGES, based on the contents of this repo.
# Requires that $GH_PAGES points to a repo checked out on the gh-pages branch.

set -e

echo "Preparing to generate documentation..."

COMMIT=$(git rev-parse HEAD)

export GIT_WORK_TREE="$GH_PAGES"
export GIT_DIR="$GH_PAGES/.git"

# Files to omit from documentation
DENYLIST_FILES=(
  date/relativecommontests.js
  events/eventtargettester.js
  # Causes an invalid use of goog.base error - disable temporarily. Dossier
  # probably just needs a release...
  goog.js
  i18n/compactnumberformatsymbolsext.js
  i18n/datetimepatternsext.js
  i18n/listsymbolsext.js
  i18n/numberformatsymbolsext.js
  promise/testsuiteadapter.js
  proto2/package_test.pb.js
  proto2/test.pb.js
  soy/soy_testhelper.js
  style/stylescrollbartester.js
  testing/parallel_closure_test_suite.js
  test_module.js
  test_module_dep.js
  tweak/testhelpers.js
  transpile.js
  useragent/useragenttestutil.js
)
declare -A DENYLIST
for file in "${DENYLIST_FILES[@]}"; do
   DENYLIST["$file"]=true
done

# Regenerate deps.js as the demos page relies on the debugloader
npm install
npm run gen_deps_js_with_tests

# Rearrange files from the repo.
cp -r doc/* "$GH_PAGES/"
mkdir -p "$GH_PAGES/source/closure"
cp -r closure/* "$GH_PAGES/source/closure/"
mkdir -p "$GH_PAGES/api"

# Download the latest js-dossier release.
npm install --no-save js-dossier

command=(
  java -jar node_modules/js-dossier/dossier.jar
  --output "$GH_PAGES/api"
  --readme scripts/ci/dossier_readme.md
  --source_url_template
      'https://github.com/google/closure-library/blob/master/%path%#L%line%'
  --type_filter '^goog\.i18n\.\w+_.*'
  --type_filter '^goog\.labs\.i18n\.\w+_.*'
  --type_filter '^.*+_$'
)

# Explicitly add all the non-denylisted files.
while read -r file; do
  if [[ ! "$file" =~ ^closure/goog/demos &&
        ! "$file" =~ ^closure/goog/debug_loader_integration_tests/testdata &&
        ! "$file" =~ _test\.js$ &&
        ! "$file" =~ _perf\.js$ &&
        "${DENYLIST[${file#closure/goog/}]}" != true ]]; then
    command=("${command[@]}" --source "$file")
  fi
done < <(find closure/goog third_party/closure/goog -name '*.js')

# Run dossier.
"${command[@]}"

# Make a commit.
git add -A
git commit -q --allow-empty -m "Latest documentation auto-pushed to gh-pages

Built from commit $COMMIT."

echo "gh-pages is ready to push in $GH_PAGES."
