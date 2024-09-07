// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"

namespace crdtp {

Binary::Binary() : bytes_(new base::RefCountedBytes) {}
Binary::Binary(const Binary& binary) : bytes_(binary.bytes_) {}
Binary::Binary(scoped_refptr<base::RefCountedMemory> bytes) : bytes_(bytes) {}
Binary::~Binary() = default;

void Binary::AppendSerialized(std::vector<uint8_t>* out) const {
  cbor::EncodeBinary(crdtp::span<uint8_t>(data(), size()), out);
}

std::string Binary::toBase64() const {
  return base::Base64Encode(base::as_string_view(*bytes_));
}

// static
Binary Binary::fromBase64(std::string_view base64, bool* success) {
  std::string decoded;
  *success = base::Base64Decode(base64, &decoded);
  return *success ? Binary::fromString(std::move(decoded)) : Binary();
}

// static
Binary Binary::fromRefCounted(scoped_refptr<base::RefCountedMemory> memory) {
  return Binary(memory);
}

// static
Binary Binary::fromVector(std::vector<uint8_t> data) {
  return Binary(base::MakeRefCounted<base::RefCountedBytes>(std::move(data)));
}

// static
Binary Binary::fromString(std::string data) {
  return Binary(base::MakeRefCounted<base::RefCountedString>(std::move(data)));
}

// static
Binary Binary::fromSpan(base::span<const uint8_t> data) {
  return Binary(base::MakeRefCounted<base::RefCountedBytes>(data));
}

// static
bool ProtocolTypeTraits<Binary>::Deserialize(DeserializerState* state,
                                             Binary* value) {
  auto* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::BINARY) {
    *value = Binary::fromSpan(tokenizer->GetBinary());
    return true;
  }
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::STRING8) {
    const auto str_span = tokenizer->GetString8();
    bool success = false;
    *value = Binary::fromBase64(
        std::string_view(reinterpret_cast<const char*>(str_span.data()),
                         str_span.size()),
        &success);
    if (!success)
      state->RegisterError(Error::BINDINGS_INVALID_BASE64_STRING);
    return success;
  }
  state->RegisterError(Error::BINDINGS_BINARY_VALUE_EXPECTED);
  return false;
}

void ProtocolTypeTraits<Binary>::Serialize(const Binary& value,
                                           std::vector<uint8_t>* bytes) {
  value.AppendSerialized(bytes);
}

// static
bool ProtocolTypeTraits<std::string>::Deserialize(DeserializerState* state,
                                                  std::string* value) {
  auto* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::STRING8) {
    const auto str_span = tokenizer->GetString8();
    *value = std::string(reinterpret_cast<const char*>(str_span.data()),
                         str_span.size());
    return true;
  }
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::STRING16) {
    const auto str = tokenizer->GetString16WireRep();
    // In Chromium, we do not support big endian architectures, so no conversion
    // is needed to interpret UTF16LE.
    base::UTF16ToUTF8(reinterpret_cast<const char16_t*>(str.data()),
                      str.size() / 2, value);
    return true;
  }
  state->RegisterError(Error::BINDINGS_STRING_VALUE_EXPECTED);
  return false;
}

namespace {

template <typename Iterable>
void SerializeDict(const Iterable& iterable, std::vector<uint8_t>* bytes) {
  ContainerSerializer serializer(bytes, cbor::EncodeIndefiniteLengthMapStart());
  for (const auto& kv : iterable) {
    serializer.AddField(span<char>(kv.first.data(), kv.first.size()),
                        kv.second);
  }
  serializer.EncodeStop();
}

bool DeserializeDict(DeserializerState* state, base::Value::Dict* dict) {
  cbor::CBORTokenizer* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::ENVELOPE)
    tokenizer->EnterEnvelope();
  if (tokenizer->TokenTag() != cbor::CBORTokenTag::MAP_START) {
    state->RegisterError(Error::CBOR_MAP_START_EXPECTED);
    return false;
  }
  tokenizer->Next();
  for (; tokenizer->TokenTag() != cbor::CBORTokenTag::STOP; tokenizer->Next()) {
    if (tokenizer->TokenTag() != cbor::CBORTokenTag::STRING8) {
      state->RegisterError(Error::CBOR_INVALID_MAP_KEY);
      return false;
    }
    auto key = tokenizer->GetString8();
    std::string name(reinterpret_cast<const char*>(key.begin()), key.size());
    tokenizer->Next();
    base::Value value;
    if (!ProtocolTypeTraits<base::Value>::Deserialize(state, &value))
      return false;
    dict->Set(name, std::move(value));
  }
  return true;
}

}  // namespace

// static
void ProtocolTypeTraits<std::string>::Serialize(const std::string& str,
                                                std::vector<uint8_t>* bytes) {
  cbor::EncodeString8(SpanFrom(str), bytes);
}

// static
bool ProtocolTypeTraits<base::Value>::Deserialize(DeserializerState* state,
                                                  base::Value* value) {
  cbor::CBORTokenizer* tokenizer = state->tokenizer();
  switch (tokenizer->TokenTag()) {
    case cbor::CBORTokenTag::TRUE_VALUE:
      *value = base::Value(true);
      break;
    case cbor::CBORTokenTag::FALSE_VALUE:
      *value = base::Value(false);
      break;
    case cbor::CBORTokenTag::NULL_VALUE:
      *value = base::Value();
      break;
    case cbor::CBORTokenTag::INT32:
      *value = base::Value(tokenizer->GetInt32());
      break;
    case cbor::CBORTokenTag::DOUBLE:
      *value = base::Value(tokenizer->GetDouble());
      break;
    case cbor::CBORTokenTag::STRING8: {
      const auto str = tokenizer->GetString8();
      *value = base::Value(std::string_view(
          reinterpret_cast<const char*>(str.data()), str.size()));
      break;
    }
    case cbor::CBORTokenTag::STRING16: {
      const auto str = tokenizer->GetString16WireRep();
      // TODO(caseq): support big-endian architectures when needed.
      *value = base::Value(std::u16string_view(
          reinterpret_cast<const char16_t*>(str.data()), str.size() / 2));
      break;
    }

    case cbor::CBORTokenTag::ENVELOPE:
      tokenizer->EnterEnvelope();
      if (tokenizer->TokenTag() != cbor::CBORTokenTag::ARRAY_START &&
          tokenizer->TokenTag() != cbor::CBORTokenTag::MAP_START) {
        state->RegisterError(Error::CBOR_MAP_OR_ARRAY_EXPECTED_IN_ENVELOPE);
        return false;
      }
      return ProtocolTypeTraits<base::Value>::Deserialize(state, value);

    case cbor::CBORTokenTag::MAP_START: {
      base::Value::Dict dict;
      if (!DeserializeDict(state, &dict))
        return false;
      *value = base::Value(std::move(dict));
      break;
    }

    case cbor::CBORTokenTag::ARRAY_START: {
      base::Value::List values;
      if (!ProtocolTypeTraits<base::Value::List>::Deserialize(state, &values)) {
        return false;
      }
      *value = base::Value(std::move(values));
      break;
    }

    // Intentionally not supported.
    case cbor::CBORTokenTag::BINARY:
    // Should not occur at this level.
    case cbor::CBORTokenTag::STOP:
    case cbor::CBORTokenTag::DONE:
      state->RegisterError(Error::CBOR_UNSUPPORTED_VALUE);
      return false;

    case cbor::CBORTokenTag::ERROR_VALUE:
      return false;
  }
  return true;
}

// static
void ProtocolTypeTraits<base::Value>::Serialize(const base::Value& value,
                                                std::vector<uint8_t>* bytes) {
  switch (value.type()) {
    case base::Value::Type::NONE:
      bytes->push_back(cbor::EncodeNull());
      return;
    case base::Value::Type::BOOLEAN:
      bytes->push_back(value.GetBool() ? cbor::EncodeTrue()
                                       : cbor::EncodeFalse());
      return;
    case base::Value::Type::INTEGER: {
      // Truncate, but DCHECK() the actual value was within int32_t range.
      // TODO(caseq): consider if we need to convert ints outside of int32_t to
      // double automatically. Right now, we maintain historic behavior where we
      // didn't.
      int32_t i = static_cast<int32_t>(value.GetInt());
      DCHECK_EQ(static_cast<int>(i), value.GetInt());
      cbor::EncodeInt32(i, bytes);
      return;
    }
    case base::Value::Type::DOUBLE:
      cbor::EncodeDouble(value.GetDouble(), bytes);
      return;
    case base::Value::Type::STRING: {
      cbor::EncodeString8(SpanFrom(value.GetString()), bytes);
      return;
    }
    case base::Value::Type::BINARY:
      // TODO(caseq): support this?
      NOTREACHED_IN_MIGRATION();
      return;
    case base::Value::Type::DICT:
      SerializeDict(value.GetDict(), bytes);
      return;
    case base::Value::Type::LIST: {
      ContainerSerializer serializer(bytes,
                                     cbor::EncodeIndefiniteLengthArrayStart());
      for (const auto& item : value.GetList())
        ProtocolTypeTraits<base::Value>::Serialize(item, bytes);
      serializer.EncodeStop();
      return;
    }
  }
}

// static
bool ProtocolTypeTraits<traits::DictionaryValue>::Deserialize(
    DeserializerState* state,
    traits::DictionaryValue* value) {
  return DeserializeDict(state, value);
}

// static
void ProtocolTypeTraits<traits::DictionaryValue>::Serialize(
    const traits::DictionaryValue& value,
    std::vector<uint8_t>* bytes) {
  SerializeDict(value, bytes);
}

// static
bool ProtocolTypeTraits<traits::ListValue>::Deserialize(
    DeserializerState* state,
    traits::ListValue* value) {
  auto* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::ENVELOPE)
    tokenizer->EnterEnvelope();
  if (tokenizer->TokenTag() != cbor::CBORTokenTag::ARRAY_START) {
    state->RegisterError(Error::CBOR_ARRAY_START_EXPECTED);
    return false;
  }
  assert(value->empty());
  tokenizer->Next();
  for (; tokenizer->TokenTag() != cbor::CBORTokenTag::STOP; tokenizer->Next()) {
    base::Value next_value;
    if (!ProtocolTypeTraits<base::Value>::Deserialize(state, &next_value))
      return false;
    value->Append(std::move(next_value));
  }
  return true;
}

void ProtocolTypeTraits<traits::ListValue>::Serialize(
    const traits::ListValue& value,
    std::vector<uint8_t>* bytes) {
  ContainerSerializer container_serializer(
      bytes, cbor::EncodeIndefiniteLengthArrayStart());
  for (const auto& item : value)
    ProtocolTypeTraits<base::Value>::Serialize(item, bytes);
  container_serializer.EncodeStop();
}

}  // namespace crdtp
