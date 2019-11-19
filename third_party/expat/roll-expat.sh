#!/bin/bash

roll_deps() {
  STEP="roll-deps" &&
  REVIEWERS=$(grep -E -v "^$|#" third_party/expat/OWNERS | paste -s -d, -)
  roll-dep -r "${REVIEWERS}" --roll-to origin/upstream/master "$@" src/third_party/expat/src/
}

update_readme() {
  STEP="update README.chromium" &&
  EXPAT_VERSION=$(git -C third_party/expat/src/ describe --long) &&
  EXPAT_COMMIT=$(git -C third_party/expat/src/ rev-parse HEAD) &&
  EXPAT_DATE=$(date "+%Y%m%d")
  sed -i'' -e "s/^Version: .*\$/Version: ${EXPAT_VERSION}/" third_party/expat/README.chromium &&
  sed -i'' -e "s/^Revision: .*\$/Revision: ${EXPAT_COMMIT}/" third_party/expat/README.chromium &&
  sed -i'' -e "s/^Date: .*\$/Date: ${EXPAT_DATE}/" third_party/expat/README.chromium &&
  git add third_party/expat/README.chromium
}

previous_rev() {
  STEP="previous revision" &&
  PREVIOUS_EXPAT_REV=$(git grep "'libexpat_revision':" HEAD~1 -- DEPS | grep -Eho "[0-9a-fA-F]{32}")
}

check_added_deleted_files() {
  previous_rev &&
  STEP="check for added or deleted files since last libexpat revision" &&
  ADDED_FILES=$(git -C third_party/expat/src/ diff --diff-filter=A --name-only ${PREVIOUS_EXPAT_REV} -- src/ | paste -s -d, -) &&
  DELETED_FILES=$(git -C third_party/expat/src/ diff --diff-filter=D --name-only ${PREVIOUS_EXPAT_REV} -- src/ | paste -s -d, -) &&
  RENAMED_FILES=$(git -C third_party/expat/src/ diff --diff-filter=R --name-only ${PREVIOUS_EXPAT_REV} -- src/ | paste -s -d, -) &&
  if [ -n "$ADDED_FILES" ]; then echo "Added files detected: " $ADDED_FILES; fi &&
  if [ -n "$DELETED_FILES" ]; then echo "Deleted files detected" $DELETED_FILES; fi &&
  if [ -n "$RENAMED_FILES" ]; then echo "Renamed files detected" $RENAMED_FILES; fi &&
  if [ -n "$ADDED_FILES" ] || [ -n "$DELETED_FILES" ] || [ -n "$RENAMED_FILES" ]; then echo -e "\nPlease update src/third_party/expat/BUILD.gn before continuing."; fi
}

commit() {
  STEP="commit" &&
  git commit --quiet --amend --no-edit
}

update_expat_config_h() {
  STEP="update expat config.h" &&
  ( cd third_party/expat/src/expat &&
    ./buildconf.sh &&
    ./configure) &&
  cp third_party/expat/src/expat/expat_config.h third_party/expat/include/expat_config/ &&
  patch -d third_party/expat -p3 < third_party/expat/0001-Do-not-claim-getrandom.patch &&
  git add third_party/expat/include/expat_config/expat_config.h
}

roll_deps "$@" &&
update_readme &&
update_expat_config_h &&
check_added_deleted_files &&
commit ||
{ echo "Failed step ${STEP}"; exit 1; }
