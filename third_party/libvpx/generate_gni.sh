#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gni files and files in the
# config/platform directories needed to build libvpx.
# Every time libvpx source code is updated just run this script.
#
# Usage:
# $ ./generate_gni.sh [--enable-avx512] [--only-configs]
#
# The following optional flags are supported:
# --enable-avx512 : Enable AVX512.
# --only-configs : Excludes generation of GN and GYP files (i.e. only
#                  configuration headers are generated).
# --disable-vp9-highbitdepth : Revert x86[_64] builds to low-bit-depth only.

export LC_ALL=C
BASE_DIR=$(pwd)
LIBVPX_SRC_DIR="source/libvpx"
LIBVPX_CONFIG_DIR="source/config"
DISABLE_AVX512="--disable-avx512"
HIGHBD="--enable-vp9-highbitdepth"

# Only disable avx512 if it is an option.
grep -q avx512 source/libvpx/configure || unset DISABLE_AVX512

for i in "$@"
do
case $i in
  --enable-avx512)
  unset DISABLE_AVX512
  shift
  ;;
  --only-configs)
  ONLY_CONFIGS=true
  shift
  ;;
  --enable-vp9-highbitdepth)
  shift
  ;;
  --disable-vp9-highbitdepth)
  unset HIGHBD
  shift
  ;;
  *)
  echo "Unknown option: $i"
  exit 1
  ;;
esac
done

# Print license header.
# $1 - Output base name
function write_license {
  echo "# This file is generated. Do not edit." >> $1
  echo "" >> $1
}

# Search for source files with the same basename in vp8, vp9, and vpx_dsp. The
# build can support such files but only when they are built into disparate
# modules. Configuring such modules for both gyp and gn are tricky so avoid the
# issue at least until gyp is removed.
function find_duplicates {
  local readonly duplicate_file_names=$(find \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp \
    -type f -name \*.c  | xargs -I {} basename {} | sort | uniq -d \
  )

  if [ -n "${duplicate_file_names}" ]; then
    echo "WARNING: DUPLICATE FILES FOUND"
    for file in  ${duplicate_file_names}; do
      find \
        $BASE_DIR/$LIBVPX_SRC_DIR/vp8 \
        $BASE_DIR/$LIBVPX_SRC_DIR/vp9 \
        $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp \
        -name $file
    done
    exit 1
  fi
}

# Generate a gni with a list of source files.
# $1 - Array name for file list. This is processed with 'declare' below to
#      regenerate the array locally.
# $2 - GN variable name.
# $3 - Output file.
function write_gni {
  # Convert the first argument back in to an array.
  declare -a file_list=("${!1}")

  echo "$2 = [" >> "$3"
  for f in $file_list
  do
    echo "  \"//third_party/libvpx/source/libvpx/$f\"," >> "$3"
  done
  echo "]" >> "$3"
}

# Convert a list of source files into gni files.
# $1 - Input file.
function convert_srcs_to_project_files {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 2. Move certain files to a separate lists to allow applying different
  #    compiler options.
  # 3. Replace .asm.s to .asm because gn will do the conversion.

  local source_list=$(grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1)

  # Not sure why vpx_config.c is not included.
  source_list=$(echo "$source_list" | grep -v 'vpx_config\.c')

  # Ignore include files.
  source_list=$(echo "$source_list" | grep -v 'x86_abi_support\.asm')

  # The actual ARM files end in .asm. We have rules to translate them to .S
  source_list=$(echo "$source_list" | sed s/\.asm\.s$/.asm/)

  # Select all x86 files ending with .c
  local intrinsic_list=$(echo "$source_list" | \
    egrep '(mmx|sse2|sse3|ssse3|sse4|avx|avx2|avx512).c$')

  # Select all neon files ending in C but only when building in RTCD mode
  if [ "libvpx_srcs_arm_neon_cpu_detect" == "$2" ]; then
    # Select all arm neon files ending in _neon.c and all asm files.
    # The asm files need to be included in the intrinsics target because
    # they need the -mfpu=neon flag.
    # the pattern may need to be updated if vpx_scale gets intrinsics
    local intrinsic_list=$(echo "$source_list" | \
      egrep 'neon.*(\.c|\.asm)$')
  fi

  # Remove these files from the main list.
  source_list=$(comm -23 <(echo "$source_list") <(echo "$intrinsic_list"))

  local x86_list=$(echo "$source_list" | egrep '/x86/')

  # Write a single .gni file that includes all source files for all archs.
  if [ 0 -ne ${#x86_list} ]; then
    local c_sources=$(echo "$source_list" | egrep '.(c|h)$')
    local assembly_sources=$(echo "$source_list" | egrep '.asm$')
    local mmx_sources=$(echo "$intrinsic_list" | grep '_mmx\.c$')
    local sse2_sources=$(echo "$intrinsic_list" | grep '_sse2\.c$')
    local sse3_sources=$(echo "$intrinsic_list" | grep '_sse3\.c$')
    local ssse3_sources=$(echo "$intrinsic_list" | grep '_ssse3\.c$')
    local sse4_1_sources=$(echo "$intrinsic_list" | grep '_sse4\.c$')
    local avx_sources=$(echo "$intrinsic_list" | grep '_avx\.c$')
    local avx2_sources=$(echo "$intrinsic_list" | grep '_avx2\.c$')
    local avx512_sources=$(echo "$intrinsic_list" | grep '_avx512\.c$')

    write_gni c_sources $2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni assembly_sources $2_assembly "$BASE_DIR/libvpx_srcs.gni"
    write_gni mmx_sources $2_mmx "$BASE_DIR/libvpx_srcs.gni"
    write_gni sse2_sources $2_sse2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni sse3_sources $2_sse3 "$BASE_DIR/libvpx_srcs.gni"
    write_gni ssse3_sources $2_ssse3 "$BASE_DIR/libvpx_srcs.gni"
    write_gni sse4_1_sources $2_sse4_1 "$BASE_DIR/libvpx_srcs.gni"
    write_gni avx_sources $2_avx "$BASE_DIR/libvpx_srcs.gni"
    write_gni avx2_sources $2_avx2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni avx512_sources $2_avx512 "$BASE_DIR/libvpx_srcs.gni"
  else
    local c_sources=$(echo "$source_list" | egrep '.(c|h)$')
    local assembly_sources=$(echo -e "$source_list\n$intrinsic_list" | \
      egrep '.asm$')
    local neon_sources=$(echo "$intrinsic_list" | grep '_neon\.c$')
    write_gni c_sources $2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni assembly_sources $2_assembly "$BASE_DIR/libvpx_srcs.gni"
    if [ 0 -ne ${#neon_sources} ]; then
      write_gni neon_sources $2_neon "$BASE_DIR/libvpx_srcs.gni"
    fi
  fi
}

# Clean files from previous make.
function make_clean {
  make clean > /dev/null
  rm -f libvpx_srcs.txt
}

# Lint a pair of vpx_config.h and vpx_config.asm to make sure they match.
# $1 - Header file directory.
function lint_config {
  # mips and native client do not contain any assembly so the headers do not
  # need to be compared to the asm.
  if [[ "$1" != *mipsel && "$1" != *mips64el && "$1" != nacl ]]; then
    $BASE_DIR/lint_config.sh \
      -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
      -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
  fi
}

# Print the configuration.
# $1 - Header file directory.
function print_config {
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
}

# Print the configuration from Header file.
# This function is an abridged version of print_config which does not use
# lint_config and it does not require existence of vpx_config.asm.
# $1 - Header file directory.
function print_config_basic {
  combined_config="$(cat $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
                   | grep -E ' +[01] *$')"
  combined_config="$(echo "$combined_config" | grep -v DO1STROUNDING)"
  combined_config="$(echo "$combined_config" | sed 's/[ \t]//g')"
  combined_config="$(echo "$combined_config" | sed 's/.*define//')"
  combined_config="$(echo "$combined_config" | sed 's/0$/=no/')"
  combined_config="$(echo "$combined_config" | sed 's/1$/=yes/')"
  echo "$combined_config" | sort | uniq
}

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
# $3 - Optional - any additional arguments to pass through.
function gen_rtcd_header {
  echo "Generate $LIBVPX_CONFIG_DIR/$1/*_rtcd.h files."
  format="clang-format -i -style=Chromium"

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
  if [[ "$2" == "mipsel" || "$2" == "mips64el" || "$2" == nacl ]]; then
    print_config_basic $1 > $BASE_DIR/$TEMP_DIR/libvpx.config
  else
    $BASE_DIR/lint_config.sh -p \
      -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
      -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm \
      -o $BASE_DIR/$TEMP_DIR/libvpx.config
  fi

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp8_rtcd $DISABLE_AVX512 $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8/common/rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp8_rtcd.h

  ${format} $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp8_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp9_rtcd $DISABLE_AVX512 $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9/common/vp9_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp9_rtcd.h

  ${format} $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp9_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_scale_rtcd $DISABLE_AVX512 $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_scale/vpx_scale_rtcd.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_scale_rtcd.h

  ${format} $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_scale_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_dsp_rtcd $DISABLE_AVX512 $3 \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp/vpx_dsp_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_dsp_rtcd.h

  ${format} $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_dsp_rtcd.h

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
}

# Generate Config files. "--enable-external-build" must be set to skip
# detection of capabilities on specific targets.
# $1 - Header file directory.
# $2 - Config command line.
function gen_config_files {
  ./configure $2 > /dev/null

  # Disable HAVE_UNISTD_H as it causes vp8 to try to detect how many cpus
  # available, which doesn't work from inside a sandbox on linux.
  sed -i.bak -e 's/\(HAVE_UNISTD_H[[:space:]]*\)1/\10/' vpx_config.h
  # Maintain old ARCH_ defines to avoid build errors because assembly file
  # dependencies are incorrect.
  sed -i.bak -e 's/\(#define \)VPX_\(ARCH_[_0-9A-Z]\+ [01]\)/&\n\1\2/' vpx_config.h

  rm vpx_config.h.bak

  # Use the correct ads2gas script.
  if [[ "$1" == linux* ]]; then
    local ASM_CONV=ads2gas.pl
  else
    local ASM_CONV=ads2gas_apple.pl
  fi

  # Generate vpx_config.asm. Do not create one for mips or native client.
  if [[ "$1" != *mipsel && "$1" != *mips64el && "$1" != nacl ]]; then
    if [[ "$1" == *x64* ]] || [[ "$1" == *ia32* ]]; then
      egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print "%define " $2 " " $3}' > vpx_config.asm
    else
      egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print $2 " EQU " $3}' | perl $BASE_DIR/$LIBVPX_SRC_DIR/build/make/$ASM_CONV > vpx_config.asm
    fi
  fi

  mkdir -p $BASE_DIR/$LIBVPX_CONFIG_DIR/$1
  cp vpx_config.* $BASE_DIR/$LIBVPX_CONFIG_DIR/$1
  make_clean
  rm -rf vpx_config.*
}

function update_readme {
  local IFS=$'\n'
  # Split git log output '<date>\n<commit hash>' on the newline to produce 2
  # array entries.
  local vals=($(git --no-pager log -1 --format="%cd%n%H" \
    --date=format:"%A %B %d %Y"))
  sed -E -i.bak \
    -e "s/^(Date:)[[:space:]]+.*$/\1 ${vals[0]}/" \
    -e "s/^(Commit:)[[:space:]]+[a-f0-9]{40}/\1 ${vals[1]}/" \
    ${BASE_DIR}/README.chromium
  rm ${BASE_DIR}/README.chromium.bak
  cat <<EOF

README.chromium updated with:
Date: ${vals[0]}
Commit: ${vals[1]}
EOF
}

find_duplicates

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

echo "Generate config files."
all_platforms="--enable-external-build"
all_platforms+=" --enable-postproc"
all_platforms+=" --enable-multi-res-encoding"
all_platforms+=" --enable-temporal-denoising"
all_platforms+=" --enable-vp9-temporal-denoising"
all_platforms+=" --enable-vp9-postproc"
all_platforms+=" --size-limit=16384x16384"
all_platforms+=" --enable-realtime-only"
all_platforms+=" --disable-install-docs"
all_platforms+=" --disable-libyuv"
x86_platforms="--enable-pic --as=yasm $DISABLE_AVX512 $HIGHBD"
gen_config_files linux/ia32 "--target=x86-linux-gcc ${all_platforms} ${x86_platforms}"
gen_config_files linux/x64 "--target=x86_64-linux-gcc ${all_platforms} ${x86_platforms}"
gen_config_files linux/arm "--target=armv7-linux-gcc --disable-neon ${all_platforms}"
gen_config_files linux/arm-neon "--target=armv7-linux-gcc ${all_platforms}"
gen_config_files linux/arm-neon-cpu-detect "--target=armv7-linux-gcc --enable-runtime-cpu-detect ${all_platforms}"
gen_config_files linux/arm64 "--target=armv8-linux-gcc ${all_platforms}"
gen_config_files linux/arm-neon-highbd "--target=armv7-linux-gcc ${all_platforms} ${HIGHBD}"
gen_config_files linux/arm64-highbd "--target=armv8-linux-gcc ${all_platforms} ${HIGHBD}"
gen_config_files linux/mipsel "--target=mips32-linux-gcc ${all_platforms}"
gen_config_files linux/mips64el "--target=mips64-linux-gcc ${all_platforms}"
gen_config_files linux/generic "--target=generic-gnu $HIGHBD ${all_platforms}"
gen_config_files win/arm64 "--target=arm64-win64-vs15 ${all_platforms} ${HIGHBD}"
gen_config_files win/ia32 "--target=x86-win32-vs14 ${all_platforms} ${x86_platforms}"
gen_config_files win/x64 "--target=x86_64-win64-vs14 ${all_platforms} ${x86_platforms}"
gen_config_files mac/ia32 "--target=x86-darwin9-gcc ${all_platforms} ${x86_platforms}"
gen_config_files mac/x64 "--target=x86_64-darwin9-gcc ${all_platforms} ${x86_platforms}"
gen_config_files ios/arm-neon "--target=armv7-linux-gcc ${all_platforms}"
gen_config_files ios/arm64 "--target=armv8-linux-gcc ${all_platforms}"
gen_config_files nacl "--target=generic-gnu $HIGHBD ${all_platforms}"

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

echo "Lint libvpx configuration."
lint_config linux/ia32
lint_config linux/x64
lint_config linux/arm
lint_config linux/arm-neon
lint_config linux/arm-neon-cpu-detect
lint_config linux/arm64
lint_config linux/arm-neon-highbd
lint_config linux/arm64-highbd
lint_config linux/mipsel
lint_config linux/mips64el
lint_config linux/generic
lint_config win/arm64
lint_config win/ia32
lint_config win/x64
lint_config mac/ia32
lint_config mac/x64
lint_config ios/arm-neon
lint_config ios/arm64
lint_config nacl

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

# chromium has required sse2 for x86 since 2014
require_sse2="--require-mmx --require-sse --require-sse2"

gen_rtcd_header linux/ia32 x86 "${require_sse2}"
gen_rtcd_header linux/x64 x86_64
gen_rtcd_header linux/arm armv7 "--disable-neon --disable-neon_asm"
gen_rtcd_header linux/arm-neon armv7
gen_rtcd_header linux/arm-neon-cpu-detect armv7
gen_rtcd_header linux/arm64 armv8
gen_rtcd_header linux/arm-neon-highbd armv7
gen_rtcd_header linux/arm64-highbd armv8
gen_rtcd_header linux/mipsel mipsel
gen_rtcd_header linux/mips64el mips64el
gen_rtcd_header linux/generic generic
gen_rtcd_header win/arm64 armv8
gen_rtcd_header win/ia32 x86 "${require_sse2}"
gen_rtcd_header win/x64 x86_64
gen_rtcd_header mac/ia32 x86 "${require_sse2}"
gen_rtcd_header mac/x64 x86_64
gen_rtcd_header ios/arm-neon armv7
gen_rtcd_header ios/arm64 armv8
gen_rtcd_header nacl nacl

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make_clean

if [ -z $ONLY_CONFIGS ]; then
  # Remove existing .gni file.
  rm -rf $BASE_DIR/libvpx_srcs.gni
  write_license $BASE_DIR/libvpx_srcs.gni

  echo "Generate X86 source list."
  config=$(print_config linux/ia32)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_x86

  # Copy vpx_version.h. The file should be the same for all platforms.
  cp vpx_version.h $BASE_DIR/$LIBVPX_CONFIG_DIR

  echo "Generate X86_64 source list."
  # Windows needs float_control_word.asm for Windows. This was previously
  # emms_mmx.asm but a refactoring pulled out the cross platform bits. Because
  # of this, use the win/x64 configuration as the reference. The empty asm
  # object should not perturb the other builds.
  config=$(print_config win/x64)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_x86_64

  echo "Generate ARM source list."
  config=$(print_config linux/arm)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm

  echo "Generate ARM NEON source list."
  config=$(print_config linux/arm-neon)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm_neon

  echo "Generate ARM NEON CPU DETECT source list."
  config=$(print_config linux/arm-neon-cpu-detect)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm_neon_cpu_detect

  echo "Generate ARM64 source list."
  config=$(print_config linux/arm64)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm64

  echo "Generate ARM NEON HighBD source list."
  config=$(print_config linux/arm-neon-highbd)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm_neon_highbd

  echo "Generate ARM64 HighBD source list."
  config=$(print_config linux/arm64-highbd)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm64_highbd

  echo "ARM64 Windows uses the ARM64 Linux HighBD source list. No need to generate it."

  echo "Generate MIPS source list."
  config=$(print_config_basic linux/mipsel)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_mips

  echo "MIPS64 source list is identical to MIPS source list. No need to generate it."

  echo "Generate NaCl source list."
  config=$(print_config_basic nacl)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_nacl

  echo "Generate GENERIC source list."
  config=$(print_config_basic linux/generic)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_generic
fi

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

gn format --in-place $BASE_DIR/BUILD.gn
gn format --in-place $BASE_DIR/libvpx_srcs.gni

cd $BASE_DIR/$LIBVPX_SRC_DIR
update_readme

cd $BASE_DIR
