

// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// n//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include <sstream>

#include <gflags/gflags.h>

#include "src/common.h"
#include "src/filesystem.h"
#include "src/sentencepiece_model.pb.h"
#include "src/sentencepiece_processor.h"

DEFINE_string(output, "", "Output filename");
DEFINE_string(model, "", "input model file name");
DEFINE_string(output_format, "vocab",
              "output format. choose from vocab or syms. vocab outputs pieces "
              "and scores, syms outputs pieces and indices.");

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  sentencepiece::SentencePieceProcessor sp;
  CHECK_OK(sp.Load(FLAGS_model));

  auto output = sentencepiece::filesystem::NewWritableFile(FLAGS_output);
  CHECK_OK(output->status());

  if (FLAGS_output_format == "vocab") {
    for (const auto &piece : sp.model_proto().pieces()) {
      std::ostringstream os;
      os << piece.piece() << "\t" << piece.score();
      output->WriteLine(os.str());
    }
  } else if (FLAGS_output_format == "syms") {
    for (int i = 0; i < sp.model_proto().pieces_size(); i++) {
      std::ostringstream os;
      os << sp.model_proto().pieces(i).piece() << "\t" << i;
      output->WriteLine(os.str());
    }
  } else {
    LOG(FATAL) << "Unsupported output format: " << FLAGS_output_format;
  }

  return 0;
}
