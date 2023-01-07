#!/bin/bash
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# `mk_hyb_file.py` is a program to generate hyphen data file for minikin.
mk_hyb=./mk_hyb_file.py
if [[ ! -f $mk_hyb_file ]]; then
  mk_hyb_url="https://android.googlesource.com/platform/frameworks/minikin/+/refs/heads/master/tools/mk_hyb_file.py"
  echo "Downloading mk_hyb_file from $mk_hyb_url"
  curl "$mk_hyb_url?format=TEXT" | base64 --decode >$mk_hyb
  chmod +x $mk_hyb
fi

# Read pattern_locales from `src/Android.mk`, expecting the format like this:
# pattern_locales := \
#     as/as \
#     be/be \
#     :
#     te/te \
#     tk/tk
# (blank line)
pattern_mk=src/Android.mk
pattern_locales () {
  awk -e '
    /^pattern_locales :=/ {flag=1; next}
    !flag {next}
    /^$/ {flag=0; next}
    {print $1}
    ' $pattern_mk
}

# Rebuild the data file in `$hyb`.
src=src/
hyb=hyb/
LICENSE=LICENSE
rm -rf "$hyb"
mkdir -p "$hyb"
rm -f "$LICENSE"

PRE=''
pattern_locales | while IFS=/ read dir name; do
  (set -x; $mk_hyb "$src$dir/hyph-$name.pat.txt" "${hyb}hyph-$name.hyb")

  # Concatenate to the LICENSE file.
  echo -e "${PRE}hyph-$name.hyb\n" >> "$LICENSE"
  for name in 'NOTICE' 'LICENSE'; do
    if [ -f "$src$dir/$name" ]; then
      echo "Appending $src$dir/$name to $LICENSE"
      cat "$src$dir/$name" >> "$LICENSE"
    fi
  done
  PRE='\n'
done

rm -f $mk_hyb
