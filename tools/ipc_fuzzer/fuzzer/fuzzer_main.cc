// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <iostream>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/string_split.h"
#include "ipc/ipc_message_macros.h"
#include "tools/ipc_fuzzer/fuzzer/fuzzer.h"
#include "tools/ipc_fuzzer/fuzzer/generator.h"
#include "tools/ipc_fuzzer/fuzzer/mutator.h"
#include "tools/ipc_fuzzer/fuzzer/rand_util.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"

namespace ipc_fuzzer {

namespace {

// TODO(mbarbella): Check to see if this value is actually reasonable.
const int kFrequency = 23;

const char kCountSwitch[] = "count";
const char kCountSwitchHelp[] =
    "Number of messages to generate (generator).";

const char kFrequencySwitch[] = "frequency";
const char kFrequencySwitchHelp[] =
    "Probability of mutation; tweak every 1/|q| times (mutator).";

const char kFuzzerNameSwitch[] = "fuzzer-name";
const char kFuzzerNameSwitchHelp[] =
    "Select from generate, mutate, or no-op. Default: generate";

const char kHelpSwitch[] = "help";
const char kHelpSwitchHelp[] =
    "Show this message.";

const char kPermuteSwitch[] = "permute";
const char kPermuteSwitchHelp[] =
    "Randomly shuffle the order of all messages (mutator).";

const char kTypeListSwitch[] = "type-list";
const char kTypeListSwitchHelp[] =
    "Explicit list of the only message-ids to mutate (mutator).";

void usage() {
  std::cerr << "Mutate messages from an exiting message file.\n";

  std::cerr << "Usage:\n"
            << "  ipc_fuzzer"
            << " [--" << kCountSwitch << "=c]"
            << " [--" << kFrequencySwitch << "=q]"
            << " [--" << kFuzzerNameSwitch << "=f]"
            << " [--" << kHelpSwitch << "]"
            << " [--" << kTypeListSwitch << "=x,y,z...]"
            << " [--" << kPermuteSwitch << "]"
            << " [infile (mutation only)] outfile\n";

  std::cerr
      << " --" << kCountSwitch << "        - " << kCountSwitchHelp << "\n"
      << " --" << kFrequencySwitch << "    - " << kFrequencySwitchHelp << "\n"
      << " --" << kFuzzerNameSwitch <<  "  - " << kFuzzerNameSwitchHelp << "\n"
      << " --" << kHelpSwitch << "         - " << kHelpSwitchHelp << "\n"
      << " --" << kTypeListSwitch <<  "    - " << kTypeListSwitchHelp << "\n"
      << " --" << kPermuteSwitch << "      - " << kPermuteSwitchHelp << "\n";
}

}  // namespace

class FuzzerFactory {
 public:
  static Fuzzer *Create(const std::string& name, int frequency) {
    if (name == "default")
      return new Generator();

    if (name == "generate")
      return new Generator();

    if (name == "mutate")
      return new Mutator(frequency);

    if (name == "no-op")
      return new NoOpFuzzer();

    std::cerr << "No such fuzzer: " << name << "\n";
    return 0;
  }
};

static std::unique_ptr<IPC::Message> RewriteMessage(IPC::Message* message,
                                                    Fuzzer* fuzzer,
                                                    FuzzerFunctionMap* map) {
  FuzzerFunctionMap::iterator it = map->find(message->type());
  if (it == map->end()) {
    // This usually indicates a missing message file in all_messages.h, or
    // that the message dump file is taken from a different revision of
    // chromium from this executable.
    std::cerr << "Unknown message type: ["
              << IPC_MESSAGE_ID_CLASS(message->type()) << ", "
              << IPC_MESSAGE_ID_LINE(message->type()) << "].\n";
    return 0;
  }

  return (*it->second)(message, fuzzer);
}

int Generate(base::CommandLine* cmd, Fuzzer* fuzzer) {
  base::CommandLine::StringVector args = cmd->GetArgs();
  if (args.size() != 1) {
    usage();
    return EXIT_FAILURE;
  }
  base::FilePath::StringType output_file_name = args[0];

  int message_count = 1000;
  if (cmd->HasSwitch(kCountSwitch))
    message_count = atoi(cmd->GetSwitchValueASCII(kCountSwitch).c_str());

  MessageVector message_vector;
  int bad_count = 0;
  if (message_count < 0) {
    // Enumerate them all.
    for (size_t i = 0; i < g_function_vector.size(); ++i) {
      std::unique_ptr<IPC::Message> new_message =
          (*g_function_vector[i])(nullptr, fuzzer);
      if (new_message)
        message_vector.push_back(std::move(new_message));
      else
        bad_count += 1;
    }
  } else {
    // Fuzz a random batch.
    for (int i = 0; i < message_count; ++i) {
      size_t index = RandInRange(g_function_vector.size());
      std::unique_ptr<IPC::Message> new_message =
          (*g_function_vector[index])(nullptr, fuzzer);
      if (new_message)
        message_vector.push_back(std::move(new_message));
      else
        bad_count += 1;
    }
  }

  std::cerr << "Failed to generate " << bad_count << " messages.\n";
  if (!MessageFile::Write(base::FilePath(output_file_name), message_vector))
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

int Mutate(base::CommandLine* cmd, Fuzzer* fuzzer) {
  base::CommandLine::StringVector args = cmd->GetArgs();
  if (args.size() != 2) {
    usage();
    return EXIT_FAILURE;
  }
  base::FilePath::StringType input_file_name = args[0];
  base::FilePath::StringType output_file_name = args[1];

  bool permute = cmd->HasSwitch(kPermuteSwitch);

  std::string type_string_list = cmd->GetSwitchValueASCII(kTypeListSwitch);
  std::vector<std::string> type_string_vector = base::SplitString(
      type_string_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::set<uint32_t> type_set;
  for (size_t i = 0; i < type_string_vector.size(); ++i) {
    type_set.insert(atoi(type_string_vector[i].c_str()));
  }

  FuzzerFunctionMap fuzz_function_map;
  PopulateFuzzerFunctionMap(&fuzz_function_map);

  MessageVector message_vector;
  if (!MessageFile::Read(base::FilePath(input_file_name), &message_vector))
    return EXIT_FAILURE;

  for (size_t i = 0; i < message_vector.size(); ++i) {
    IPC::Message* msg = message_vector[i].get();
    // If an explicit type set is specified, make sure we should be mutating
    // this message type on this run.
    if (!type_set.empty() && !base::Contains(type_set, msg->type())) {
      continue;
    }
    std::unique_ptr<IPC::Message> new_message =
        RewriteMessage(msg, fuzzer, &fuzz_function_map);
    if (new_message)
      message_vector[i] = std::move(new_message);
  }

  if (permute) {
    std::shuffle(message_vector.begin(), message_vector.end(),
                 *g_mersenne_twister);
  }

  if (!MessageFile::Write(base::FilePath(output_file_name), message_vector))
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

int FuzzerMain(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = cmd->GetArgs();

  if (args.size() == 0 || args.size() > 2 || cmd->HasSwitch(kHelpSwitch)) {
    usage();
    return EXIT_FAILURE;
  }

  InitRand();

  PopulateFuzzerFunctionVector(&g_function_vector);
  std::cerr << "Counted " << g_function_vector.size()
            << " distinct messages present in chrome.\n";

  std::string fuzzer_name = "default";
  if (cmd->HasSwitch(kFuzzerNameSwitch))
    fuzzer_name = cmd->GetSwitchValueASCII(kFuzzerNameSwitch);

  int frequency = kFrequency;
  if (cmd->HasSwitch(kFrequencySwitch))
    frequency = atoi(cmd->GetSwitchValueASCII(kFrequencySwitch).c_str());

  Fuzzer* fuzzer = FuzzerFactory::Create(fuzzer_name, frequency);
  if (!fuzzer)
    return EXIT_FAILURE;

  int result;
  base::FilePath::StringType output_file_name;
  if (fuzzer_name == "default" || fuzzer_name == "generate") {
    result = Generate(cmd, fuzzer);
  } else {
    result = Mutate(cmd, fuzzer);
  }

  return result;
}

}  // namespace ipc_fuzzer

int main(int argc, char** argv) {
  return ipc_fuzzer::FuzzerMain(argc, argv);
}
