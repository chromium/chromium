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

#include <algorithm>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "model_interface.h"
#include "sentencepiece_model.pb.h"
#include "util.h"

namespace sentencepiece {

ModelInterface::ModelInterface(const ModelProto& model_proto)
    : model_proto_(&model_proto), status_(util::OkStatus()) {}
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
  std::vector<bool> byte_found(256, false);

  for (int i = 0; i < model_proto_->pieces_size(); ++i) {
    const auto &sp = model_proto_->pieces(i);
    if (sp.piece().empty()) {
      status_ = util::InternalError("piece must not be empty.");
      return;
    }

    const bool is_normal_piece =
        (sp.type() == ModelProto::SentencePiece::NORMAL ||
         sp.type() == ModelProto::SentencePiece::USER_DEFINED ||
         sp.type() == ModelProto::SentencePiece::UNUSED);
    if (!port::InsertIfNotPresent(
            is_normal_piece ? &pieces_ : &reserved_id_map_, sp.piece(), i)) {
      status_ = util::InternalError(sp.piece() + " is already defined.");
      return;
    }

    if (sp.type() == ModelProto::SentencePiece::USER_DEFINED) {
      user_defined_symbols.insert(sp.piece());
    }

    if (sp.type() == ModelProto::SentencePiece::UNKNOWN) {
      if (unk_id_ >= 0) {
        status_ = util::InternalError("unk is already defined.");
        return;
      }
      unk_id_ = i;
    }

    if (sp.type() == ModelProto::SentencePiece::BYTE) {
      if (!model_proto_->trainer_spec().byte_fallback()) {
        status_ =
            util::InternalError("byte piece " + sp.piece() +
                                " is found although `byte_fallback` is false.");
        return;
      }
      const int byte = PieceToByte(sp.piece());
      if (0 <= byte && byte < 256) {
        byte_found[byte] = true;
      } else {
        status_ =
            util::InternalError("byte piece " + sp.piece() + " is invalid.");
        return;
      }
    }
  }

  if (unk_id_ == -1) {
    status_ = util::InternalError("unk is not defined.");
    return;
  }

  if (model_proto_->trainer_spec().byte_fallback()) {
    // Checks that there are 256 byte pieces.
    if (std::find(byte_found.begin(), byte_found.end(), false) !=
        byte_found.end()) {
      status_ = util::InternalError(
          "there are not 256 byte pieces although `byte_fallback` is true.");
      return;
    }
  }

  matcher_ = absl::make_unique<normalizer::PrefixMatcher>(user_defined_symbols);
}

std::vector<absl::string_view> SplitIntoWords(absl::string_view text,
                                              bool treat_ws_as_suffix,
                                              bool allow_ws_only_pieces) {
  const char *begin = text.data();
  const char *end = text.data() + text.size();

  // Space symbol (U+2581)
  const absl::string_view kSpaceSymbol = "\xe2\x96\x81";
  bool in_ws_sequence = false;

  std::vector<absl::string_view> result;
  if (treat_ws_as_suffix) {  // put ws tokens at the end of non-ws sequences.
    if (begin < end) result.emplace_back(begin, 0);
    while (begin < end) {
      const int mblen =
          std::min<int>(string_util::OneCharLen(begin), end - begin);
      const bool is_ws = absl::string_view(begin, mblen) == kSpaceSymbol;

      if (is_ws) {  // keep track of sequences consecutive ws tokens.
        in_ws_sequence = true;
      } else if (in_ws_sequence) {
        if (allow_ws_only_pieces) {
          result.emplace_back(begin, 0);
        }

        in_ws_sequence = false;
      }

      result.back() =
          absl::string_view(result.back().data(), result.back().size() + mblen);
      begin += mblen;

      if (begin < end && is_ws && !allow_ws_only_pieces) {
        result.emplace_back(begin, 0);
      }
    }
  } else {
    while (begin < end) {
      const int mblen =
          std::min<int>(string_util::OneCharLen(begin), end - begin);
      bool is_ws = absl::string_view(begin, mblen) == kSpaceSymbol;

      // if is whitespace (and not in sequence if allow_ws_only_pieces is True)
      if (begin == text.data() ||
          (is_ws && (!in_ws_sequence || !allow_ws_only_pieces))) {
        result.emplace_back(begin, 0);  // add empty string piece.
        in_ws_sequence = true;
      }

      if (in_ws_sequence && !is_ws) {
        in_ws_sequence = false;
      }

      result.back() =
          absl::string_view(result.back().data(), result.back().size() + mblen);
      begin += mblen;
    }
  }

  return result;
}

std::string ByteToPiece(unsigned char c) {
  return absl::StrFormat("<0x%02X>", c);
}

int PieceToByte(absl::string_view piece) {
  using PieceToByteMap = absl::flat_hash_map<std::string, unsigned char>;
  static const auto* const kMap = []() -> PieceToByteMap* {
    auto* m = new PieceToByteMap();
    for (int i = 0; i < 256; ++i) {
      (*m)[ByteToPiece(i)] = i;
    }
    return m;
  }();
  const auto it = kMap->find(std::string(piece));
  if (it == kMap->end()) {
    return -1;
  } else {
    return it->second;
  }
}

}  // namespace sentencepiece
