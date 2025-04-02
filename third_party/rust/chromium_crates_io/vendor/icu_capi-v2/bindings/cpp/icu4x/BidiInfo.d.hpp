#ifndef icu4x_BidiInfo_D_HPP
#define icu4x_BidiInfo_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct BidiParagraph; }
class BidiParagraph;
}


namespace icu4x {
namespace capi {
    struct BidiInfo;
} // namespace capi
} // namespace

namespace icu4x {
class BidiInfo {
public:

  inline size_t paragraph_count() const;

  inline std::unique_ptr<icu4x::BidiParagraph> paragraph_at(size_t n) const;

  inline size_t size() const;

  inline uint8_t level_at(size_t pos) const;

  inline const icu4x::capi::BidiInfo* AsFFI() const;
  inline icu4x::capi::BidiInfo* AsFFI();
  inline static const icu4x::BidiInfo* FromFFI(const icu4x::capi::BidiInfo* ptr);
  inline static icu4x::BidiInfo* FromFFI(icu4x::capi::BidiInfo* ptr);
  inline static void operator delete(void* ptr);
private:
  BidiInfo() = delete;
  BidiInfo(const icu4x::BidiInfo&) = delete;
  BidiInfo(icu4x::BidiInfo&&) noexcept = delete;
  BidiInfo operator=(const icu4x::BidiInfo&) = delete;
  BidiInfo operator=(icu4x::BidiInfo&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_BidiInfo_D_HPP
