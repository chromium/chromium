#!/bin/bash
#
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gni files and files in the
# config/platform directories needed to build libaom.
#
# Every time the upstream source code is updated this script must be run.
#
# Usage:
# $ ./cmake_update.sh
# Requirements:
# Install the following Debian packages.
# - cmake3
# - yasm or nasm
# Toolchain for armv7:
# - gcc-arm-linux-gnueabihf
# - g++-arm-linux-gnueabihf
# Toolchain for arm64:
# - gcc-aarch64-linux-gnu
# - g++-aarch64-linux-gnu
# 32bit build environment for cmake. Including but potentially not limited to:
# - lib32gcc-7-dev
# - lib32stdc++-7-dev
# Alternatively: treat 32bit builds like Windows and manually tweak aom_config.h

set -eE

# sort() consistently.
export LC_ALL=C

BASE=$(pwd)
SRC="${BASE}/source/libaom"
CFG="${BASE}/source/config"

function clean {
  rm -rf "${TMP}"
}

# Create empty temp and config directories.
# $1 - Header file directory.
function reset_dirs {
  cd ..
  rm -rf "${TMP}"
  mkdir "${TMP}"
  cd "${TMP}"

  echo "Generate ${1} config files."
  rm -fr "${CFG}/${1}"
  mkdir -p "${CFG}/${1}/config"
}

if [ $# -ne 0 ]; then
  echo "Unknown option(s): ${@}"
  exit 1
fi

# Missing function:
# find_duplicates
# We may have enough targets to avoid re-implementing this.

# Generate Config files.
# $1 - Header file directory.
# $2 - cmake options.
function gen_config_files {
  cmake "${SRC}" ${2} &> cmake.txt

  case "${1}" in
    *x64*|*ia32*)
      egrep "#define [A-Z0-9_]+ [01]" config/aom_config.h | \
        awk '{print "%define " $2 " " $3}' > config/aom_config.asm
      ;;
  esac

  cp config/aom_config.{h,c,asm} "${CFG}/${1}/config/"

  cp config/*_rtcd.h "${CFG}/${1}/config/"
}

function update_readme {
  local IFS=$'\n'
  # Split git log output '<date>\n<commit hash>' on the newline to produce 2
  # array entries.
  local vals=($(git -C "${SRC}" --no-pager log -1 --format="%cd%n%H" \
    --date=format:"%A %B %d %Y"))
  sed -E -i.bak \
    -e "s/^(Date:)[[:space:]]+.*$/\1 ${vals[0]}/" \
    -e "s/^(Commit:)[[:space:]]+[a-f0-9]{40}/\1 ${vals[1]}/" \
    ${BASE}/README.chromium
  rm ${BASE}/README.chromium.bak
  cat <<EOF

README.chromium updated with:
Date: ${vals[0]}
Commit: ${vals[1]}
EOF
}

# Update aom_config.h to support Windows instead of linux because cmake doesn't
# generate VS project files on linux.
#
# $1 - File to modify.
function convert_to_windows {
  sed -i.bak \
    -e 's/\(#define[[:space:]]INLINE[[:space:]]*\)inline/\1 __inline/' \
    -e 's/\(#define[[:space:]]HAVE_PTHREAD_H[[:space:]]*\)1/\1 0/' \
    -e 's/\(#define[[:space:]]HAVE_UNISTD_H[[:space:]]*\)1/\1 0/' \
    -e 's/\(#define[[:space:]]CONFIG_GCC[[:space:]]*\)1/\1 0/' \
    -e 's/\(#define[[:space:]]CONFIG_MSVS[[:space:]]*\)0/\1 1/' \
    "${1}"
  rm "${1}.bak"
}

# Scope 'trap' error reporting to configuration generation.
(
TMP=$(mktemp -d "${BASE}/build.XXXX")
cd "${TMP}"

trap '{
  [ -f ${TMP}/cmake.txt ] && cat ${TMP}/cmake.txt
  echo "Build directory ${TMP} not removed automatically."
}' ERR

all_platforms="-DCONFIG_SIZE_LIMIT=1"
all_platforms+=" -DDECODE_HEIGHT_LIMIT=16384 -DDECODE_WIDTH_LIMIT=16384"
all_platforms+=" -DCONFIG_AV1_ENCODER=0"
all_platforms+=" -DCONFIG_LOWBITDEPTH=1"
all_platforms+=" -DCONFIG_MAX_DECODE_PROFILE=0"
all_platforms+=" -DCONFIG_NORMAL_TILE_MODE=1"
all_platforms+=" -DCONFIG_LIBYUV=0"
# avx2 optimizations account for ~0.3mb of the decoder.
#all_platforms+=" -DENABLE_AVX2=0"
toolchain="-DCMAKE_TOOLCHAIN_FILE=${SRC}/build/cmake/toolchains"

reset_dirs linux/generic
gen_config_files linux/generic "-DAOM_TARGET_CPU=generic ${all_platforms}"
# libaom_srcs.gni and aom_version.h are shared.
cp libaom_srcs.gni "${BASE}"
cp config/aom_version.h "${CFG}/config/"

reset_dirs linux/ia32
gen_config_files linux/ia32 "${toolchain}/x86-linux.cmake ${all_platforms} \
  -DCONFIG_PIC=1 \
  -DAOM_RTCD_FLAGS=--require-mmx;--require-sse;--require-sse2"

reset_dirs linux/x64
gen_config_files linux/x64 "${all_platforms}"

# Copy linux configurations and modify for Windows.
reset_dirs win/ia32
cp "${CFG}/linux/ia32/config"/* "${CFG}/win/ia32/config/"
convert_to_windows "${CFG}/win/ia32/config/aom_config.h"
egrep "#define [A-Z0-9_]+[[:space:]]+[01]" "${CFG}/win/ia32/config/aom_config.h" \
  | awk '{print "%define " $2 " " $3}' > "${CFG}/win/ia32/config/aom_config.asm"

# Copy linux configurations and modify for Windows.
reset_dirs win/x64
cp "${CFG}/linux/x64/config"/* "${CFG}/win/x64/config/"
convert_to_windows "${CFG}/win/x64/config/aom_config.h"
egrep "#define [A-Z0-9_]+[[:space:]]+[01]" "${CFG}/win/x64/config/aom_config.h" \
  | awk '{print "%define " $2 " " $3}' > "${CFG}/win/x64/config/aom_config.asm"

reset_dirs linux/arm
gen_config_files linux/arm \
  "${toolchain}/armv7-linux-gcc.cmake -DENABLE_NEON=0 ${all_platforms}"

reset_dirs linux/arm-neon
gen_config_files linux/arm-neon "${toolchain}/armv7-linux-gcc.cmake ${all_platforms}"

reset_dirs linux/arm-neon-cpu-detect
gen_config_files linux/arm-neon-cpu-detect \
  "${toolchain}/armv7-linux-gcc.cmake -DCONFIG_RUNTIME_CPU_DETECT=1 ${all_platforms}"

reset_dirs linux/arm64
gen_config_files linux/arm64 "${toolchain}/arm64-linux-gcc.cmake ${all_platforms}"

# Copy linux configurations and modify for Windows.
reset_dirs win/arm64
cp "${CFG}/linux/arm64/config"/* "${CFG}/win/arm64/config/"
convert_to_windows "${CFG}/win/arm64/config/aom_config.h"
)

update_readme

git cl format > /dev/null \
  || echo "ERROR: 'git cl format' failed. Please run 'git cl format' manually."

clean
