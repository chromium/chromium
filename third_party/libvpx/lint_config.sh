#!/bin/bash
#
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to compare vpx_config.h and vpx_config.asm to
# verify the two files match.
#
# Arguments:
#
# -h - C Header file.
# -a - ASM file.
# -p - Print the options if correct.
# -o - Output file.
#
# Usage:
#
# # Compare the two configuration files and output the final results.
# ./lint_config.sh -h vpx_config.h -a vpx_config.asm -o libvpx.config -p

set -e

export LC_ALL=C
print_final="no"

while getopts "h:a:o:p" flag; do
  if [[ "$flag" == "h" ]]; then
    header_file=$OPTARG
  elif [[ "$flag" == "a" ]]; then
    asm_file=$OPTARG
  elif [[ "$flag" == "o" ]]; then
    out_file=$OPTARG
  elif [[ "$flag" == "p" ]]; then
    print_final="yes"
  fi
done

if [[ -z "$header_file" ]]; then
  echo "Header file not specified."
  exit 1
fi

if [[ -z "$asm_file" ]]; then
  echo "ASM file not specified."
  exit 1
fi

# Concat header file and assembly file and select those ended with 0 or 1.
combined_config="$(cat $header_file $asm_file | grep -E ' +[01] *$')"

# Extra filtering for known exceptions.
combined_config="$(echo "$combined_config" | grep -v WIDE_REFERENCE)"
combined_config="$(echo "$combined_config" | grep -v ARCHITECTURE)"
combined_config="$(echo "$combined_config" | grep -v DO1STROUNDING)"

# Remove all spaces.
combined_config="$(echo "$combined_config" | sed 's/[[:space:]]//g')"

# Remove #define in the header file.
combined_config="$(echo "$combined_config" | sed 's/.*define//')"

# Remove equ in the ASM file.
combined_config="$(echo "$combined_config" | sed 's/\.equ//')" # gas style
combined_config="$(echo "$combined_config" | sed 's/equ//')" # rvds style
combined_config="$(echo "$combined_config" | sed 's/\.set//')" # apple style

# Remove %define in YASM ASM files.
combined_config="$(echo "$combined_config" | sed 's/%define[[:space:]]*//')"

# Remove useless comma in gas style assembly file.
combined_config="$(echo "$combined_config" | sed 's/,//')"

# Substitute 0 with =no.
combined_config="$(echo "$combined_config" | sed 's/0$/=no/')"

# Substitute 1 with =yes.
combined_config="$(echo "$combined_config" | sed 's/1$/=yes/')"

# Find the mismatch variables.
odd_config="$(echo "$combined_config" | sort | uniq -u)"
odd_vars="$(echo "$odd_config" | sed 's/=.*//' | uniq)"

for var in $odd_vars; do
  echo "Error: Configuration mismatch for $var."
  echo "Header file: $header_file"
  echo "$(cat -n $header_file | grep "$var[ \t]")"
  echo "Assembly file: $asm_file"
  echo "$(cat -n $asm_file | grep "$var[ \t]")"
  echo ""
done

if [[ -n "$odd_vars" ]]; then
  exit 1
fi

if [[ "$print_final" == "no" ]]; then
  exit
fi

# Do some additional filter to make libvpx happy.
combined_config="$(echo "$combined_config" | grep -v ARCH_X86=no)"
combined_config="$(echo "$combined_config" | grep -v ARCH_X86_64=no)"

# Print out the unique configurations.
if [[ -n "$out_file" ]]; then
  echo "$combined_config" | sort | uniq > $out_file
else
  echo "$combined_config" | sort | uniq
fi
