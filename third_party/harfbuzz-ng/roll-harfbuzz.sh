#!/bin/bash

rolldeps() {
  STEP="roll-deps" &&
  REVIEWERS=$(grep -E -v "^$|#" third_party/harfbuzz-ng/OWNERS | paste -s -d, -) &&
  roll-dep -r "${REVIEWERS}" --roll-to origin/upstream/master "$@" src/third_party/harfbuzz-ng/src/
}

updatereadme() {
  STEP="update README.chromium" &&
  HBVERSION=$(git -C third_party/harfbuzz-ng/src/ describe --long) &&
  HBCOMMIT=$(git -C third_party/harfbuzz-ng/src/ rev-parse HEAD) &&
  HBDATE=$(date "+%Y%m%d")
  sed -i'' -e "s/^Version: .*\$/Version: ${HBVERSION%-*}/" third_party/harfbuzz-ng/README.chromium &&
  sed -i'' -e "s/^Revision: .*\$/Revision: ${HBCOMMIT}/" third_party/harfbuzz-ng/README.chromium &&
  sed -i'' -e "s/^Date: .*\$/Date: ${HBDATE}/" third_party/harfbuzz-ng/README.chromium &&
  git add third_party/harfbuzz-ng/README.chromium
}

previousrev() {
  STEP="original revision" &&
  PREVIOUS_HARFBUZZ_REV=$(git grep "'harfbuzz_revision':" HEAD~1 -- DEPS | grep -Eho "[0-9a-fA-F]{32}")
}

check_added_deleted_files() {
  STEP="Check for added or deleted files since last HarfBuzz revision" &&
  previousrev &&
  ADDED_FILES=$(git -C third_party/harfbuzz-ng/src/ diff --diff-filter=A --name-only ${PREVIOUS_HARFBUZZ_REV} -- src/ | paste -s -d, -) &&
  DELETED_FILES=$(git -C third_party/harfbuzz-ng/src/ diff --diff-filter=D --name-only ${PREVIOUS_HARFBUZZ_REV} -- src/ | paste -s -d, -) &&
  RENAMED_FILES=$(git -C third_party/harfbuzz-ng/src/ diff --diff-filter=R --name-only ${PREVIOUS_HARFBUZZ_REV} -- src/ | paste -s -d, -) &&
  if [ -n "$ADDED_FILES" ]; then echo "Added files detected: " $ADDED_FILES; fi &&
  if [ -n "$DELETED_FILES" ]; then echo "Deleted files detected" $DELETED_FILES; fi &&
  if [ -n "$RENAMED_FILES" ]; then echo "Renamed files detected" $RENAMED_FILES; fi &&
  if [ -n "$ADDED_FILES" ] || [ -n "$DELETED_FILES" ] || [ -n "$RENAMED_FILES" ]; then echo -e "\nPlease update src/third_party/harfbuzz-ng/BUILD.gn before continuing."; fi
}

commit() {
  STEP="commit" &&
  git commit --quiet --amend --no-edit
}

rolldeps "$@" &&
updatereadme &&
check_added_deleted_files &&
commit ||
{ echo "Failed step ${STEP}"; exit 1; }
