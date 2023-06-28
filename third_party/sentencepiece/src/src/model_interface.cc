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

#include "src/model_interface.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "src/sentencepiece_model.pb.h"
#include "src/util.h"

namespace sentencepiece {

ModelInterface::ModelInterface(const ModelProto &model_proto)
    : model_proto_(&model_proto), status_(::util::OkStatus()) {}
ModelInterface::~ModelInterface() {}

#define RETURN_PIECE(name, default_value)                                \
  if (model_proto_->trainer_spec().name().empty()) return default_value; \
  return model_proto_->trainer_spec().name();

absl::string_view ModelInterface::unk_piece() const {
  RETURN_PIECE(unk_piece, "<unk>");
}

absl::string_view ModelInterface::bos_piece() const {
  RETURN_PIECE(bos_piece, "<s>");
}

absl::string_view ModelInterface::eos_piece() const {
  RETURN_PIECE(eos_piece, "</s>");
}

absl::string_view ModelInterface::pad_piece() const {
  RETURN_PIECE(pad_piece, "<pad>");
}

#undef RETURN_PIECE

int ModelInterface::PieceToId(absl::string_view piece) const {
  auto it = reserved_id_map_.find(piece);
  if (it != reserved_id_map_.end()) {
    return it->second;
  }
  auto it2 = pieces_.find(piece);
  if (it2 != pieces_.end()) {
    return it2->second;
  }
  return unk_id_;
}

void ModelInterface::InitializePieces() {
  pieces_.clear();
  reserved_id_map_.clear();
  unk_id_ = -1;

  std::set<absl::string_view> user_defined_symbols;

  for (int i = 0; i < model_proto_->pieces_size(); ++i) {
    const auto &sp = model_proto_->pieces(i);
    if (sp.piece().empty()) {
      status_ = ::util::InternalError("piece must not be empty.");
      return;
    }

    const bool is_normal_piece =
        (sp.type() == ModelProto::SentencePiece::NORMAL ||
         sp.type() == ModelProto::SentencePiece::USER_DEFINED ||
         sp.type() == ModelProto::SentencePiece::UNUSED);
    if (!port::InsertIfNotPresent(
            is_normal_piece ? &pieces_ : &reserved_id_map_, sp.piece(), i)) {
      status_ = ::util::InternalError(sp.piece() + " is already defined.");
      return;
    }

    if (sp.type() == ModelProto::SentencePiece::USER_DEFINED) {
      user_defined_symbols.insert(sp.piece());
    }

    if (sp.type() == ModelProto::SentencePiece::UNKNOWN) {
      if (unk_id_ >= 0) {
        status_ = ::util::InternalError("unk is already defined.");
        return;
      }
      unk_id_ = i;
    }
  }

  if (unk_id_ == -1) {
    status_ = ::util::InternalError("unk is not defined.");
    return;
  }

  matcher_ = absl::make_unique<normalizer::PrefixMatcher>(user_defined_symbols);
}

std::vector<absl::string_view> SplitIntoWords(absl::string_view text,
                                              bool treat_whitespace_as_suffix) {
  const char *begin = text.data();
  const char *end = text.data() + text.size();

  // Space symbol (U+2581)
  const absl::string_view kSpaceSymbol = "\xe2\x96\x81";

  std::vector<absl::string_view> result;
  if (treat_whitespace_as_suffix) {
    if (begin < end) result.emplace_back(begin, 0);
    while (begin < end) {
      const int mblen =
          std::min<int>(string_util::OneCharLen(begin), end - begin);
      const bool is_ws = absl::string_view(begin, mblen) == kSpaceSymbol;
      result.back() =
          absl::string_view(result.back().data(), result.back().size() + mblen);
      begin += mblen;
      if (begin < end && is_ws) result.emplace_back(begin, 0);
    }
  } else {
    while (begin < end) {
      const int mblen =
          std::min<int>(string_util::OneCharLen(begin), end - begin);
      if (begin == text.data() ||
          absl::string_view(begin, mblen) == kSpaceSymbol)
        result.emplace_back(begin, 0);  // add empty string piece.
      result.back() =
          absl::string_view(result.back().data(), result.back().size() + mblen);
      begin += mblen;
    }
  }

  return result;
}

}  // namespace sentencepiece
