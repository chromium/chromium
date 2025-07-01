#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to use clang complier errors to enclose UNSAFE_TODO() regions.

usage() {
    CMD="$(basename $0)"
    echo "Usage: $CMD [-h] [-C build_dir] [-t target]... <directory>"
    echo "Options:"
    echo "  -C build_dir   Specify the build directory, defaults to out/Debug"
    echo "  -t target      Build target besides chrome (multiple times OK)"
    echo "  -h             Display this help message and exit."
    echo "Arguments:"
    echo "  directory      Directory under src/ to checkout and modify."
    exit 1
}

DIRECTORY=""
BUILD_DIR="out/Debug"
TARGETS="chrome"

PARSED_OPTIONS=$(getopt -o C:t:h -- "$@")
if [ $? -ne 0 ]; then
    echo "Error: Invalid command line arguments." >&2
    usage
fi

eval set -- "$PARSED_OPTIONS"

while true; do
    case "$1" in
        -C)
            BUILD_DIR="$2"
            shift 2
            ;;
        -t)
            TARGETS="$TARGETS $2"
            shift 2
            ;;
        -h)
            usage
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "Internal error!" >&2
            exit 1
            ;;
    esac
done

if [ -n "$1" ]; then # Check if a directory argument exists
    DIRECTORY="$1"
    shift
else
    echo "Error: Missing required <directory> argument." >&2
    usage
fi

# Check for any unexpected extra arguments
if [ -n "$1" ]; then
    echo "Error: Too many arguments. Unexpected argument(s): $@" >&2
    usage
fi

echo "Checking GN build arg configuration"
DIAGNOSTICS_ARG="$(gn args -C $BUILD_DIR --short \
                   --list=diagnostics_print_source_range_info)"
if [[ $DIAGNOSTICS_ARG != *true ]] ; then
    echo "Set GN arg diagnostics_print_source_range_info = true"
    exit 1
fi

WARNINGS_ARG="$(gn args -C $BUILD_DIR --list=treat_warnings_as_errors --short)"
if [[ $WARNINGS_ARG != *false ]] ; then
    echo "Set GN arg treat_warnings_as_errors = false"
    exit 1
fi

TMPDIR="$(mktemp -d)"
echo "Temporary files will be written to $TMPDIR"
echo

SOURCE_FILES="$(git grep -l pragma.allow_unsafe_ $DIRECTORY | grep '\.cc$')"
if [[ -z "$SOURCE_FILES" ]] ; then
   echo "No files met criteria"
   exit 1
fi

echo "Initial set of source files:"
echo "$SOURCE_FILES"
echo
tools/clang/unsafe_pragma_rewriter/remove_unsafe_pragma.py $SOURCE_FILES

IFFY_FILES="$(grep -l '#if' $SOURCE_FILES)"
if [[ ! -z "$IFFY_FILES" ]] ; then
  echo "Reverting conditionally-compiled files:"
  echo "$IFFY_FILES"
  echo ""
  git checkout -- $IFFY_FILES
fi

git diff --name-only | sort > $TMPDIR/filelist1
SOURCE_FILES="$(cat $TMPDIR/filelist1)"
if [[ -z "$SOURCE_FILES" ]] ; then
  echo "No remaining files"
  exit 1
fi
echo "Remaining files:"
echo "$SOURCE_FILES"
echo ""

echo "Compile to find unsafe errors ..."
autoninja -k 1000 -C $BUILD_DIR -v $TARGETS > $TMPDIR/buildlog0

cat $TMPDIR/buildlog0 | \
    tools/clang/unsafe_pragma_rewriter/extract_sources.py | \
    sort > $TMPDIR/filelist2

# Set source files to files that were compiled
SOURCE_FILES="$(comm -12 $TMPDIR/filelist1 $TMPDIR/filelist2)"
if [[ -z "$SOURCE_FILES" ]] ; then
    echo "No relevant files were compiled"
    exit 1
fi
echo "Set of files compiled"
echo "$SOURCE_FILES"
echo ""

echo "Resetting to clean state ..."
git checkout -- $DIRECTORY

REWRITE_FILES="$(cat $TMPDIR/buildlog0 | \
                 tools/clang/unsafe_pragma_rewriter/extract_failures.py)"
if [[ -z "$REWRITE_FILES" ]] ; then
   echo "No files met criteria for rewriting"
   exit 1
fi

tools/clang/unsafe_pragma_rewriter/remove_unsafe_pragma.py $SOURCE_FILES
cat $TMPDIR/buildlog0 | tools/clang/unsafe_pragma_rewriter/fix_unsafe.py

NEEDS_HEADER="$(git grep -l UNSAFE_TODO $SOURCE_FILES)"
tools/add_header.py --header '"base/compiler_specific.h"' $NEEDS_HEADER

for i in 1 2 3 4 ; do
    echo "Compile to find bad rewrites (Pass ${i}) ..."
    autoninja -k 1000 -C $BUILD_DIR $TARGETS > $TMPDIR/buildlog$i
    FAILURES="$(cat $TMPDIR/buildlog$i | \
                tools/clang/unsafe_pragma_rewriter/extract_failures.py)"
    if [[ -z "$FAILURES" ]] ; then
        break
    fi
    echo "Failed to compile, reverting:"
    for FAILURE in $FAILURES ; do
        echo "$FAILURE"
        git checkout -- $FAILURE
    done
    echo ""
done

echo "Formatting changes"
git cl format

echo "Finished."
