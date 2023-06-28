#!/usr/bin/perl

# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Generate unicode_sciript_data.h from Unicode Scripts.txt
#
# usage: ./gen_unicode_Scripts_code.pl < scripts > unicode_script_data.h
#
print "#ifndef UNICODE_SCRIPT_DATA_H_\n";
print "#define UNICODE_SCRIPT_DATA_H_\n";
print "namespace sentencepiece {\n";
print "namespace unicode_script {\n";
print "namespace {\n";
print "void InitTable(std::unordered_map<char32, ScriptType> *smap) {\n";
print "  CHECK_NOTNULL(smap)->clear();\n";

while (<>) {
  chomp;
  if (/^([0-9A-F]+)\s+;\s+(\S+)\s+\#/) {
    printf("  (*smap)[0x%s] = U_%s;\n", $1, $2);
  } elsif (/^([0-9A-F]+)\.\.([0-9A-F]+)\s+;\s+(\S+)\s+\#/) {
    printf("  for (char32 c = 0x%s; c <= 0x%s; ++c)\n", $1, $2);
    printf("    (*smap)[c] = U_%s;\n", $3);
  } else {
    next;
  }
}

print "}\n";
print "}  // namespace\n";
print "}  // namespace unicode_script\n";
print "}  // namespace sentencepiece\n";
print "#endif  // UNICODE_SCRIPT_DATA_H_\n";
