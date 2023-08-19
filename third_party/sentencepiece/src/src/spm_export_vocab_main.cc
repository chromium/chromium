// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include <sstream>

#include "absl/flags/flag.h"
#include "common.h"
#include "filesystem.h"
#include "init.h"
#include "sentencepiece_model.pb.h"
#include "sentencepiece_processor.h"

ABSL_FLAG(std::string, output, "", "Output filename");
ABSL_FLAG(std::string, model, "", "input model file name");
ABSL_FLAG(std::string,
          output_format,
          "vocab",
          "output format. choose from vocab or syms. vocab outputs pieces "
          "and scores, syms outputs pieces and indices.");

int main(int argc, char *argv[]) {
  sentencepiece::ScopedResourceDestructor cleaner;
  sentencepiece::ParseCommandLineFlags(argv[0], &argc, &argv, true);

  sentencepiece::SentencePieceProcessor sp;
  CHECK_OK(sp.Load(absl::GetFlag(FLAGS_model)));

  auto output =
      sentencepiece::filesystem::NewWritableFile(absl::GetFlag(FLAGS_output));
  CHECK_OK(output->status());

  if (absl::GetFlag(FLAGS_output_format) == "vocab") {
    for (const auto &piece : sp.model_proto().pieces()) {
      std::ostringstream os;
      os << piece.piece() << "\t" << piece.score();
      output->WriteLine(os.str());
    }
  } else if (absl::GetFlag(FLAGS_output_format) == "syms") {
    for (int i = 0; i < sp.model_proto().pieces_size(); i++) {
      std::ostringstream os;
      os << sp.model_proto().pieces(i).piece() << "\t" << i;
      output->WriteLine(os.str());
    }
  } else {
    LOG(FATAL) << "Unsupported output format: "
               << absl::GetFlag(FLAGS_output_format);
  }

  return 0;
}
