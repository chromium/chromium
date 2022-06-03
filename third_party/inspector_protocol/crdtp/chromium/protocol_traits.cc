#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

#include <utility>
#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
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
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(bytes_->front()),
                        bytes_->size()),
      &encoded);
  return encoded;
}

// static
Binary Binary::fromBase64(base::StringPiece base64, bool* success) {
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
  return Binary(base::RefCountedBytes::TakeVector(&data));
}

// static
Binary Binary::fromString(std::string data) {
  return Binary(base::RefCountedString::TakeString(&data));
}

// static
Binary Binary::fromSpan(const uint8_t* data, size_t size) {
  return Binary(scoped_refptr<base::RefCountedBytes>(
      new base::RefCountedBytes(data, size)));
}

// static
bool ProtocolTypeTraits<Binary>::Deserialize(DeserializerState* state,
                                             Binary* value) {
  auto* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::BINARY) {
    const span<uint8_t> bin = tokenizer->GetBinary();
    *value = Binary::fromSpan(bin.data(), bin.size());
    return true;
  }
  if (tokenizer->TokenTag() == cbor::CBORTokenTag::STRING8) {
    const auto str_span = tokenizer->GetString8();
    bool success = false;
    *value = Binary::fromBase64(
        base::StringPiece(reinterpret_cast<const char*>(str_span.data()),
                          str_span.size()),
        &success);
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

// static
void ProtocolTypeTraits<std::string>::Serialize(const std::string& str,
                                                std::vector<uint8_t>* bytes) {
  cbor::EncodeString8(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(str.data()), str.size()),
      bytes);
}

}  // namespace crdtp
