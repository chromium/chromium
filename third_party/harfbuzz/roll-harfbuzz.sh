#!/bin/bash

rolldeps() {
  STEP="roll-deps" &&
  REVIEWERS=$(grep -E -v "^$|#" third_party/harfbuzz/OWNERS | paste -s -d, -) &&
  roll-dep -r "${REVIEWERS}" --roll-to origin/upstream/main "$@" src/third_party/harfbuzz/src/
}

updatereadme() {
  STEP="update README.chromium" &&
  HB_VERSION=$(git -C third_party/harfbuzz/src/ describe --long) &&
  HB_COMMIT=$(git -C third_party/harfbuzz/src/ rev-parse HEAD) &&
  HB_DATE=$(date "+%Y-%m-%d")
  HB_CPE_VERSION=$(echo ${HB_VERSION} | sed -r -e's/^([0-9]+)\.([0-9]+)\.([0-9]+)-[0-9]+-g[0-9a-f]+$/\1.\2.\3/') &&
  [ ${HB_VERSION} != ${HB_CPE_VERSION} ] &&
  sed -i'' -e "s/^Version: .*\$/Version: ${HB_VERSION%-*}/" third_party/harfbuzz/README.chromium &&
  sed -i'' -e "s@^CPEPrefix: cpe:/a:harfbuzz_project:harfbuzz:.*\$@CPEPrefix: cpe:/a:harfbuzz_project:harfbuzz:${HB_CPE_VERSION}@" third_party/harfbuzz/README.chromium &&
  sed -i'' -e "s/^Revision: .*\$/Revision: ${HB_COMMIT}/" third_party/harfbuzz/README.chromium &&
  sed -i'' -e "s/^Date: .*\$/Date: ${HB_DATE}/" third_party/harfbuzz/README.chromium &&
  git add third_party/harfbuzz/README.chromium
}

previousrev() {
  STEP="original revision" &&
  PREVIOUS_HARFBUZZ_REV=$(git grep "'harfbuzz_revision':" HEAD~1 -- DEPS | grep -Eho "[0-9a-fA-F]{32,40}")
}

check_added_deleted_files() {
  previousrev &&
  STEP="Check for added or deleted files since last HarfBuzz revision" &&
  ADDED_FILES=$(git -C third_party/harfbuzz/src/ diff --diff-filter=A --name-only ${PREVIOUS_HARFBUZZ_REV} -- src/ | paste -s -d, -) &&
  DELETED_FILES=$(git -C third_party/harfbuzz/src/ diff --diff-filter=D --name-only ${PREVIOUS_HARFBUZZ_REV} -- src/ | paste -s -d, -) &&
  RENAMED_FILES=$(git -C third_party/harfbuzz/src/ diff --diff-filter=R --name-only ${PREVIOUS_HARFBUZZ_REV} -- src/ | paste -s -d, -) &&
  if [ -n "$ADDED_FILES" ]; then echo "Added files detected: " $ADDED_FILES; fi &&
  if [ -n "$DELETED_FILES" ]; then echo "Deleted files detected" $DELETED_FILES; fi &&
  if [ -n "$RENAMED_FILES" ]; then echo "Renamed files detected" $RENAMED_FILES; fi &&
  if [ -n "$ADDED_FILES" ] || [ -n "$DELETED_FILES" ] || [ -n "$RENAMED_FILES" ]; then echo -e "\nPlease update src/third_party/harfbuzz/BUILD.gn before continuing."; fi
}

check_all_files_are_categorized() {
  #for each file name in src/src/hb-*.{cc,h,hh}
  #  if the file name is not present in BUILD.gn
  #    should be added to BUILD.gn (in 'unused_sources' if unwanted)

  #for each file name \"src/src/.*\" in BUILD.gn
  #  if the file name does not exist
  #    should be removed from BUILD.gn

  STEP="Updating BUILD.gn" &&
  ( # Create subshell for CDPATH, and cd.
    CDPATH= && cd -- "$(dirname -- "$0")" &&

    if grep -q "have not been categorized" BUILD.gn; then
      sed -i '/have not been categorized/d' BUILD.gn
    fi

    HB_SOURCE_MISSING=()
    while IFS= read -r HB_SOURCE; do
      if ! grep -qF "\"$HB_SOURCE\"" BUILD.gn; then
        HB_SOURCE_MISSING+=("$HB_SOURCE")
      fi
    done < <(find src/src -type f \( -name "hb-*.cc" -o -name "hb-*.h" -o -name "hb-*.hh" \) | sort)

    GN_SOURCE_MISSING=()
    while IFS= read -r GN_SOURCE; do
      if [ ! -f "$GN_SOURCE" ]; then
        GN_SOURCE_MISSING+=("$GN_SOURCE")
      fi
    done < <(grep -oE "\"src/src/[^\"]+\"" BUILD.gn | sed 's/^.\(.*\).$/\1/' | sort -u)

    if [ ${#HB_SOURCE_MISSING[@]} -gt 0 ]; then
      echo "Is in src/src/hb-*.{cc,h,hh} but not in BUILD.gn:"
      for f in "${HB_SOURCE_MISSING[@]}"; do
        echo "      \"$f\","
      done
    fi

    if [ ${#GN_SOURCE_MISSING[@]} -gt 0 ]; then
      echo "Is referenced in BUILD.gn but does not exist:"
      for f in "${GN_SOURCE_MISSING[@]}"; do
        echo "\"$f\""
      done
    fi

    if [ ${#HB_SOURCE_MISSING[@]} -gt 0 ] || [ ${#GN_SOURCE_MISSING[@]} -gt 0 ]; then
      echo -e "\nA failing assertion was added to BUILD.gn. Please categorize the files into 'sources' or 'unused_sources' and remove the assertion."
      printf '\nassert(false, "Added or deleted files have not been categorized as used or unused sources: Added in upstream: %s Missing in upstream: %s")\n' "${HB_SOURCE_MISSING[*]}" "${GN_SOURCE_MISSING[*]}" >> BUILD.gn &&
      git add BUILD.gn
    fi
  )
}

commit() {
  STEP="commit" &&
  git commit --quiet --amend --no-edit
}

rolldeps "$@" &&
updatereadme &&
check_added_deleted_files &&
check_all_files_are_categorized &&
commit ||
{ echo "Failed step ${STEP}"; exit 1; }
