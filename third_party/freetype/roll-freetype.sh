#!/bin/bash

rolldeps() {
  STEP="roll-deps" &&
  REVIEWERS=$(paste -s -d, third_party/freetype/OWNERS) &&
  roll-dep -r "${REVIEWERS}" "$@" src/third_party/freetype/src/
}

addtrybots() {
  STEP="add trybots" &&
  (git show -s --format=%B HEAD \
    | git interpret-trailers --trailer "Cq-Include-Trybots:luci.chromium.try:linux_chromium_msan_rel_ng" \
    | git commit --amend -F -)
}

addotherprojectbugs() {
  STEP="add pdfium bug" &&
  (git show -s --format=%B HEAD \
    | git interpret-trailers --trailer "PDFium-Issue:pdfium:" \
    | git commit --amend -F -)
}

updatereadme() {
  STEP="update README.chromium" &&
  FTVERSION=$(git -C third_party/freetype/src/ describe --long) &&
  FTCOMMIT=$(git -C third_party/freetype/src/ rev-parse HEAD) &&
  sed -i'' -e "s/^Version: .*\$/Version: ${FTVERSION%-*}/" third_party/freetype/README.chromium &&
  sed -i'' -e "s/^Revision: .*\$/Revision: ${FTCOMMIT}/" third_party/freetype/README.chromium &&
  git add third_party/freetype/README.chromium
}

previousrev() {
  STEP="original revision" &&
  PREVIOUS_FREETYPE_REV=$(git grep "'freetype_revision':" HEAD~1 -- DEPS | grep -Eho "[0-9a-fA-F]{32}")
}

mergeinclude() {
  INCLUDE=$1 &&
  previousrev &&
  STEP="merge ${INCLUDE}: check for merge conflicts" &&
  TMPFILE=$(mktemp) &&
  git -C third_party/freetype/src/ cat-file blob ${PREVIOUS_FREETYPE_REV}:include/${INCLUDE} >> ${TMPFILE} &&
  git merge-file third_party/freetype/include/freetype-custom/${INCLUDE} ${TMPFILE} third_party/freetype/src/include/${INCLUDE} &&
  rm ${TMPFILE} &&
  git add third_party/freetype/include/freetype-custom/${INCLUDE}
}

checkmodules() {
  previousrev &&
  STEP="check modules.cfg: check list of modules and dependencies" &&
  ! git -C third_party/freetype/src/ diff --name-only ${PREVIOUS_FREETYPE_REV} | grep -q modules.cfg
}

commit() {
  STEP="commit" &&
  git commit --quiet --amend --no-edit
}

rolldeps "$@" &&
addtrybots &&
addotherprojectbugs &&
updatereadme &&
mergeinclude "freetype/config/ftoption.h" &&
mergeinclude "freetype/config/public-macros.h" &&
checkmodules &&
commit ||
{ echo "Failed step ${STEP}"; exit 1; }
