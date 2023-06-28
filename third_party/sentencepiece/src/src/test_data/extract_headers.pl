#!/usr/bin/perl

# Copyright 2018 Google Inc.
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

# Extract header files required for build protobuf-lite
#
# usage: ./extract_headers.pl *.cc

use strict;
use warnings;

sub Process() {
  my $file = shift @_;
  if ($file =~ /\.h$/) {
    print "$file\n";
  }
  return unless open(F, $file);
  my @files = ();
  while (<F>) {
    chomp;
    if (/\#include <(google\/protobuf\/[^>]+)>/) {
      push @files, $1;
    }
  }
  close(F);
  for my $file (@files) {
    &Process($file);
  }
}

for my $f (@ARGV) {
  &Process($f);
}
