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

# Generate spec_parser.h from sentencepiece_model.proto
#
# usage: ./gen_spec_parser.pl sentencepiece_model.proto > spec_parser.h

use strict;
use warnings;

sub ProcessPrinter() {
  my ($filename) = @_;
  my $classname = "";
  my $valid = 0;
  my %enum;
  open(F, $filename) || die;
  print "namespace {\n";
  while (<F>) {
    chomp;
    if (/^\s*message (\S+)/) {
      $classname = $1;
      $valid = 0;
      if ($classname =~ /(TrainerSpec|NormalizerSpec)/) {
        print "inline std::string PrintProto(const $classname &message) {\n";
        print "  std::ostringstream os;\n\n";
        print "  os << \"$classname {\\n\";\n";
        $valid = 1;
      }
    } elsif (/^\s*}/) {
      next if (!$valid);
      print "  os << \"}\\n\";\n";
      print "\n  return os.str();\n";
      print "}\n\n";
    } elsif (/enum\s*(\S+)/) {
      my $name = $1;
      $enum{$name} = 1;
      next if (!$valid);
      print "  static const std::map<$classname::$name, std::string> k${name}_Map = { ";
      while (<F>) {
        if (/(\S+)\s*=\s*(\d+)/) {
          print "{$classname::$1, \"$1\"}, ";
        } elsif (/}/) {
          print " };\n";
          last;
        }
      }
    } elsif (/\s*(repeated|optional)\s+(\S+)\s+(\S+)\s*=\s*(\d+)/) {
      next if (/deprecated = true/);
      next if (!$valid);
      my $opt = $1;
      my $type = $2;
      my $name = $3;
      if ($type =~ /(int|double|float|bool|string)/) {
        if ($opt eq "optional") {
          print "  os << \"  ${name}: \" << message.${name}() << \"\\n\";\n";
        } else {
          print "  for (const auto &v : message.${name}())\n";
          print "    os << \"  ${name}: \" << v << \"\\n\";\n";
        }
      } elsif (defined $enum{$type}) {
        if ($opt eq "optional") {
          print "  {\n";
          print "    const auto it = k${type}_Map.find(message.${name}());\n";
          print "    if (it == k${type}_Map.end())\n";
          print "      os << \"  ${name}: unknown\\n\";\n";
          print "    else\n";
          print "      os << \"  ${name}: \" << it->second << \"\\n\";\n";
          print "  }\n";
        } else {
          print "  for (const auto &v : message.${name}()) {\n";
          print "    const auto it = k${type}_Map.find(v);\n";
          print "    if (it == k${type}_Map.end())\n";
          print "      os << \"  ${name}: unknown\\n\";\n";
          print "    else\n";
          print "      os << \"  ${name}: \" << it->second << \"\\n\";\n";
          print "   }\n";
        }
      }
    }
  }
  print "}  // namespace\n\n";
  close(F);
}

sub ProcessParser() {
  my ($filename) = @_;
  my $classname = "";
  my $valid = 0;
  my %enum;
  open(F, $filename) || die;
  while (<F>) {
    if (/^\s*message (\S+)/) {
      $classname = $1;
      $valid = 0;
      if ($classname =~ /(TrainerSpec|NormalizerSpec)/) {
        print "util::Status SentencePieceTrainer::SetProtoField(const std::string& name, const std::string& value, $classname *message) {\n";
        print "  CHECK_OR_RETURN(message);\n\n";
        $valid = 1;
      }
    } elsif (/^\s*}/) {
      next if (!$valid);
      print "  return util::StatusBuilder(util::error::NOT_FOUND)\n";
      print "    << \"unknown field name \\\"\" << name << \"\\\" in ${classname}.\";\n";
      print "}\n\n";
    } elsif (/enum\s*(\S+)/) {
      my $name = $1;
      $enum{$name} = 1;
      next if (!$valid);
      print "  static const std::map <std::string, $classname::$name> k${name}_Map = { ";
      while (<F>) {
        if (/(\S+)\s*=\s*(\d+)/) {
          print "{\"$1\", $classname::$1}, ";
        } elsif (/}/) {
          print " };\n\n";
          last;
        }
      }
    } elsif (/\s*(repeated|optional)\s+(\S+)\s+(\S+)\s*=\s*(\d+)/) {
      next if (/deprecated = true/);
      next if (!$valid);
      my $opt = $1;
      my $type = $2;
      my $name = $3;
      my $func_prefix = $opt eq "optional" ? "set_" : "add_";
      my $body = "";
      if ($type =~ /(int|double|float|bool)/) {
        my $empty = $type eq "bool" ? "\"true\"" : "\"\"";
        $body =
          "${type} v;\n" .
          "    if (!string_util::lexical_cast(val.empty() ? ${empty} : val, &v))\n" .
          "      return util::StatusBuilder(util::error::INVALID_ARGUMENT) << \"cannot parse \\\"\" << val << \"\\\" as ${type}.\";\n" .
          "    message->${func_prefix}${name}(v);\n";
      } elsif ($type =~ /string/) {
        $body = "message->${func_prefix}${name}(val);\n";
      } elsif ($type =~ /bytes/) {
        $body = "message->${func_prefix}${name}(val.data(), val.size());\n";
      } elsif (defined $enum{$type}) {
        $body = "const auto it = k${type}_Map.find(string_util::ToUpper(val));\n" .
          "    if (it == k${type}_Map.end())\n" .
          "      return util::StatusBuilder(util::error::INVALID_ARGUMENT) << \"unknown enumeration value of \\\"\" << val << \"\\\" as ${type}.\";\n" .
          "    message->${func_prefix}${name}(it->second);\n";
      }
      print "  if (name == \"${name}\") {\n";
      if ($opt eq "repeated") {
        print "    for (const auto &val : string_util::Split(value, \",\")) {\n";
        print "      ${body}";
        print "    }\n";
      } else {
        print "    const auto &val = value;\n";
        print "    ${body}";
      }
      print "    return util::OkStatus();\n";
      print "  }\n\n";
    }
  }
  close(F);
}

for my $file (@ARGV) {
  &ProcessPrinter($file);
  &ProcessParser($file);
}
