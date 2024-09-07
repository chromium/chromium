#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# For more fine-grained instructions, see:
# https://docs.google.com/document/d/1chTvr3fSofQNV_PDPEHRyUgcJCQBgTDOOBriW9gIm9M/edit?ts=5e9549a2#heading=h.fjdnrdg1gcty

set -u  # unset variables are quit-worthy errors

# Possible flag values and their default settings.
PLATFORMS="linux"
BUILD_CLANG=true
REWRITE=true
EXTRACT_EDITS=true
KEEP_BUILD=false
INCREMENTAL_CLANG_BUILD=false
# The command line string to parse them.
LONG_FLAGS="platforms::,skip-building-clang,skip-rewrite,skip-extract-edits,\
           keep-build,incremental-clang-build,help"
options=$(getopt -l "$LONG_FLAGS" -o "p::brekih" -- "$@")

# Important not to access $1 if there is no passed values (due to set -u above
# will trigger an error).
# This ensures that '--' is the last value and will break the loop below.
eval set -- "$options"
while true; do
  case "$1" in
    -p|--platforms)
      shift
      PLATFORMS="$1"
      ;;
    -b|--skip-building-clang)
      BUILD_CLANG=false
      KEEP_BUILD=true
      ;;
    -r|--skip-rewrite)
      REWRITE=false
      ;;
    -e|--skip-extract-edits)
      EXTRACT_EDITS=false
      ;;
    -k|--keep-build)
      KEEP_BUILD=true
      ;;
    -i|--incremental-clang-build)
      INCREMENTAL_CLANG_BUILD=true
      KEEP_BUILD=true
      ;;
    -h|--help)
      echo "Usage: rewrite-multiple-platforms.sh [OPTIONS]..."
      echo "Runs the clang plugin spanify over selected chromium platforms and"\
           " spanifies unsafe buffers usage."
      echo ""
      echo "Options:"
      echo "  [-p|--platforms]=<comma,separated,list,of,platforms>, defaults"\
           "to 'linux'"
      echo "  [-b|--skip-building-clang], when set we won't attempt to build"\
           "third_party's clang, requires a previous run with --keep-build,"\
           "but this flag also sets --keep-build for convenience."
      echo "  [-r|--skip-rewrite], when set we won't create/destroy the"\
           "scratch directory and won't run the clang plugin."
      echo "  [-e|--skip-extract-edits], when set we won't run"\
           "extract_edits.py to generate the patches."
      echo "  [-k|--keep-build], when set we won't restore third_party/llvm."\
           "this should be set when you are going to run this script again,"\
           "but note you need to restore the directory to prevent slow down of"\
           "your regular chromium builds."
      echo "  [-i|--incremental-clang-build], when set we attempt to build"\
           "incrementally. This only works if you previously completed a run"\
           "with --keep-build and you haven't rebased, or gclient sync'd."\
           "This also sets --keep-build for convenience."
      echo ""
      echo "Check the README for more info."
      exit 0
      ;;
    --)
      shift
      break;;
  esac
  shift
done

# Create the scratch directory that we need and ensure it is empty.
if [ $REWRITE = true ]
then
  if [ -d ~/scratch ]
  then
    echo "*** Clearing ~/scratch ***"
    rm -r ~/scratch
  fi
  mkdir ~/scratch
fi

COMPILE_DIRS=.
EDIT_DIRS=.

# Build and test the rewriter.
if [ $BUILD_CLANG = true ]
then
  # Save llvm-build as it is about to be overwritten (if it hasn't already been
  # saved).
  if [ ! -d third_party/llvm-build-upstream/ ]
  then
    echo "*** Saving current build ***"

    mv third_party/llvm-build third_party/llvm-build-upstream
  else
    echo "*** Build is already saved ***"
  fi
  if [ $INCREMENTAL_CLANG_BUILD = true ]
  then
    echo "*** Building the rewriter incrementally ***"
    time ninja -C third_party/llvm-build/Release+Asserts/
  else
    echo "*** Building the rewriter completely ***"
    time tools/clang/scripts/build.py \
        --with-android \
        --without-fuchsia \
        --extra-tools spanify || exit 1

  fi
else
  echo "*** Skipping building clang ***"
fi

echo "*** Testing the rewriter ***"
tools/clang/spanify/tests/run_all_tests.py || exit 1

args_for_platform() {
    case "$1" in

    android)
        cat <<EOF
target_os = "android"
clang_use_chrome_plugins = false
is_chrome_branded = true
is_debug = false
dcheck_always_on = true
is_official_build = true
symbol_level = 1
use_remoteexec = false
enable_remoting = true
enable_webview_bundles = true
ffmpeg_branding = "Chrome"
proprietary_codecs = true
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    win)
        cat <<EOF
target_os = "win"
clang_use_chrome_plugins = false
enable_precompiled_headers = false
is_chrome_branded = true
is_debug = false
dcheck_always_on = true
is_official_build = true
symbol_level = 1
use_remoteexec = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    linux)
        cat <<EOF
target_os = "linux"
clang_use_chrome_plugins = false
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_remoteexec = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    cros)
        cat <<EOF
target_os = "chromeos"
chromeos_is_browser_only = true
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_remoteexec = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    ash)
        cat <<EOF
target_os = "chromeos"
dcheck_always_on = true
is_debug = false
is_official_build = true
use_remoteexec = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    mac)
        cat <<EOF
target_os = "mac"
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_remoteexec = false
chrome_pgo_phase = 0
symbol_level = 1
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    *)
        echo "unknown platform"
        exit 1
        ;;
    esac
}

# The latest rewrite directory.
OUT_DIR=""

pre_process() {
    PLATFORM="$1"
    OUT_DIR="out/rewrite-$PLATFORM"

    mkdir -p "$OUT_DIR"
    args_for_platform "$PLATFORM" > "$OUT_DIR/args.gn"

    # Build generated files that a successful compilation depends on.
    echo "*** Preparing targets for $PLATFORM ***"
    gn gen $OUT_DIR
    time ninja -C $OUT_DIR -t targets all \
        | grep '^gen/.*\(\.h\|inc\|css_tokenizer_codepoints.cc\)' \
        | cut -d : -f 1 \
        | xargs -s $(expr $(getconf ARG_MAX) - 256) ninja -C $OUT_DIR \
        || exit 1


    TARGET_OS_OPTION=""
    if [ $PLATFORM = "win" ]; then
        TARGET_OS_OPTION="--target_os=win"
    fi
}

main_rewrite() {
    PLATFORM=$1
    OUT_DIR="out/rewrite-${PLATFORM}"

    TARGET_OS_OPTION=""
    if [ $PLATFORM = "win" ]; then
        TARGET_OS_OPTION="--target_os=win"
    fi

    # Main rewrite.
    echo "*** Running the main rewrite phase for $PLATFORM ***"
    time tools/clang/scripts/run_tool.py \
        $TARGET_OS_OPTION \
        --tool spanify \
        --generate-compdb \
        -p $OUT_DIR \
        $COMPILE_DIRS > ~/scratch/rewriter-$PLATFORM.main.out
    touch ~/scratch/rewriter.main.out
    cat ~/scratch/rewriter-$PLATFORM.main.out >> ~/scratch/rewriter.main.out
}

if [ $REWRITE = true ]
then
  for PLATFORM in ${PLATFORMS//,/ }
  do
      pre_process "$PLATFORM"
  done

  for PLATFORM in ${PLATFORMS//,/ }
  do
      main_rewrite "$PLATFORM"
  done
else
  echo "*** Skipping rewrite ***"
fi

# Apply edits generated by the main rewrite.
if [ $EXTRACT_EDITS = true ]
then
  echo "*** Clearing test patches ***"
  rm ~/scratch/patch*

  echo "*** Applying edits ***"
  cat ~/scratch/rewriter.main.out | \
      tools/clang/spanify/extract_edits.py | \
      tools/clang/scripts/apply_edits.py -p $OUT_DIR $EDIT_DIRS
else
  echo "*** Skipping edits ***"
fi

# Format sources, as many lines are likely over 80 chars now.
echo "*** Formatting ***"
time git cl format

# Restore llvm-build. Without this, your future builds will be painfully slow.
if [ $KEEP_BUILD = false ]
then
  echo "*** Restoring llvm-build you will need to build the plugin again ***"
  rm -r -f third_party/llvm-build
  mv third_party/llvm-build-upstream third_party/llvm-build
else
  echo "*** Not restoring llvm-build, chromium wide builds will be slower ***"
fi
