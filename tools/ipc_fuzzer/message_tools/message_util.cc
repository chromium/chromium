// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "third_party/re2/src/re2/re2.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"
#include "tools/ipc_fuzzer/message_lib/message_names.h"

namespace {

const char kDumpSwitch[] = "dump";
const char kDumpSwitchHelp[] =
    "dump human-readable form to stdout instead of copying.";

const char kEndSwitch[] = "end";
const char kEndSwitchHelp[] =
    "output messages before |m|th message in file (exclusive).";

const char kHelpSwitch[] = "help";
const char kHelpSwitchHelp[] =
    "display this message.";

const char kInSwitch[] = "in";
const char kInSwitchHelp[] =
    "output only the messages at the specified positions in the file.";

const char kInvertSwitch[] = "invert";
const char kInvertSwitchHelp[] =
    "output messages NOT meeting above criteria.";

const char kRegexpSwitch[] = "regexp";
const char kRegexpSwitchHelp[] =
    "output messages matching regular expression |x|.";

const char kStartSwitch[] = "start";
const char kStartSwitchHelp[] =
    "output messages after |n|th message in file (inclusive).";

void usage() {
  std::cerr << "ipc_message_util: Concatenate all |infile| message files and "
            << "copy a subset of the result to |outfile|.\n";

  std::cerr << "Usage:\n"
            << "  ipc_message_util"
            << " [--" << kStartSwitch << "=n]"
            << " [--" << kEndSwitch << "=m]"
            << " [--" << kInSwitch << "=i[,j,...]]"
            << " [--" << kRegexpSwitch << "=x]"
            << " [--" << kInvertSwitch << "]"
            << " [--" << kDumpSwitch << "]"
            << " [--" << kHelpSwitch << "]"
            << " infile,infile,... [outfile]\n";

  std::cerr << "    --" << kStartSwitch << "  - " << kStartSwitchHelp << "\n"
            << "    --" << kEndSwitch << "    - " << kEndSwitchHelp << "\n"
            << "    --" << kInSwitch << "     - " << kInSwitchHelp << "\n"
            << "    --" << kRegexpSwitch << " - " << kRegexpSwitchHelp << "\n"
            << "    --" << kInvertSwitch << " - " << kInvertSwitchHelp << "\n"
            << "    --" << kDumpSwitch << "   - " << kDumpSwitchHelp << "\n"
            << "    --" << kHelpSwitch << "   - " << kHelpSwitchHelp << "\n";
}

std::string MessageName(const IPC::Message* msg) {
  return ipc_fuzzer::MessageNames::GetInstance()->TypeToName(msg->type());
}

bool MessageMatches(const IPC::Message* msg, const RE2& pattern) {
  return RE2::FullMatch(MessageName(msg), pattern);
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = cmd->GetArgs();

  if (args.size() < 1 || args.size() > 2 || cmd->HasSwitch(kHelpSwitch)) {
    usage();
    return EXIT_FAILURE;
  }

  size_t start_index = 0;
  if (cmd->HasSwitch(kStartSwitch)) {
    int temp = atoi(cmd->GetSwitchValueASCII(kStartSwitch).c_str());
    if (temp > 0)
      start_index = static_cast<size_t>(temp);
  }

  size_t end_index = INT_MAX;
  if (cmd->HasSwitch(kEndSwitch)) {
    int temp = atoi(cmd->GetSwitchValueASCII(kEndSwitch).c_str());
    if (temp > 0)
      end_index = static_cast<size_t>(temp);
  }

  bool has_regexp = cmd->HasSwitch(kRegexpSwitch);
  RE2 filter_pattern(cmd->GetSwitchValueASCII(kRegexpSwitch));

  bool invert = cmd->HasSwitch(kInvertSwitch);
  bool perform_dump = cmd->HasSwitch(kDumpSwitch);

  base::FilePath::StringType output_file_name;

  if (!perform_dump) {
    if (args.size() < 2) {
      usage();
      return EXIT_FAILURE;
    }
    output_file_name = args[1];
  }

  ipc_fuzzer::MessageVector input_message_vector;
  for (const base::FilePath::StringType& name : base::SplitString(
           args[0], base::FilePath::StringType(1, ','),
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    ipc_fuzzer::MessageVector message_vector;
    if (!ipc_fuzzer::MessageFile::Read(base::FilePath(name), &message_vector)) {
      return EXIT_FAILURE;
    }
    input_message_vector.insert(input_message_vector.end(),
                                std::make_move_iterator(message_vector.begin()),
                                std::make_move_iterator(message_vector.end()));
  }

  bool has_indices = cmd->HasSwitch(kInSwitch);
  std::vector<bool> indices;

  if (has_indices) {
    indices.resize(input_message_vector.size(), false);
    for (const std::string& cur : base::SplitString(
             cmd->GetSwitchValueASCII(kInSwitch), ",", base::TRIM_WHITESPACE,
             base::SPLIT_WANT_ALL)) {
      int index = atoi(cur.c_str());
      if (index >= 0 && static_cast<size_t>(index) < indices.size())
        indices[index] = true;
    }
  }

  ipc_fuzzer::MessageVector output_message_vector;
  std::vector<size_t> remap_vector;

  for (size_t i = 0; i < input_message_vector.size(); ++i) {
    bool valid = (i >= start_index && i < end_index);
    if (valid && has_regexp) {
      valid = MessageMatches(input_message_vector[i].get(), filter_pattern);
    }
    if (valid && has_indices) {
      valid = indices[i];
    }
    if (valid != invert) {
      output_message_vector.push_back(std::move(input_message_vector[i]));
      remap_vector.push_back(i);
    }
  }

  if (perform_dump) {
    for (size_t i = 0; i < output_message_vector.size(); ++i) {
      std::cout << remap_vector[i] << ". "
                << MessageName(output_message_vector[i].get()) << "\n";
    }
  } else {
    if (!ipc_fuzzer::MessageFile::Write(
            base::FilePath(output_file_name), output_message_vector)) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
