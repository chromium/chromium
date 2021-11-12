#ifndef CRDTP_CHROMIUM_PROTOCOL_TYPE_TRAITS_H_
#define CRDTP_CHROMIUM_PROTOCOL_TYPE_TRAITS_H_

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/inspector_protocol/crdtp/protocol_core.h"
#include "third_party/inspector_protocol/crdtp/serializable.h"

namespace base {
class Value;
}

namespace crdtp {
class Serializable;

template <>
struct CRDTP_EXPORT ProtocolTypeTraits<std::string> {
  static bool Deserialize(DeserializerState* state, std::string* value);
  static void Serialize(const std::string& value, std::vector<uint8_t>* bytes);
};

// A read-only sequence of uninterpreted bytes with reference-counted storage.
class CRDTP_EXPORT Binary : public Serializable {
 public:
  Binary(const Binary&);
  Binary();
  ~Binary() override;

  // Implements Serializable.
  void AppendSerialized(std::vector<uint8_t>* out) const override;

  const uint8_t* data() const { return bytes_->front(); }
  size_t size() const { return bytes_->size(); }
  scoped_refptr<base::RefCountedMemory> bytes() const { return bytes_; }

  std::string toBase64() const;

  static Binary fromBase64(base::StringPiece base64, bool* success);
  static Binary fromRefCounted(scoped_refptr<base::RefCountedMemory> memory);
  static Binary fromVector(std::vector<uint8_t> data);
  static Binary fromString(std::string data);
  static Binary fromSpan(const uint8_t* data, size_t size);

 private:
  explicit Binary(scoped_refptr<base::RefCountedMemory> bytes);
  scoped_refptr<base::RefCountedMemory> bytes_;
};

template <>
struct CRDTP_EXPORT ProtocolTypeTraits<Binary> {
  static bool Deserialize(DeserializerState* state, Binary* value);
  static void Serialize(const Binary& value, std::vector<uint8_t>* bytes);
};

template <>
struct detail::MaybeTypedef<Binary> {
  typedef ValueMaybe<Binary> type;
};

}  // namespace crdtp

#endif  // CRDTP_CHROMIUM_PROTOCOL_TYPE_TRAITS_H_
