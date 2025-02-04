#ifndef icu4x_BidiParagraph_D_HPP
#define icu4x_BidiParagraph_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
class BidiDirection;
}


namespace icu4x {
namespace capi {
    struct BidiParagraph;
} // namespace capi
} // namespace

namespace icu4x {
class BidiParagraph {
public:

  inline bool set_paragraph_in_text(size_t n);

  inline icu4x::BidiDirection direction() const;

  inline size_t size() const;

  inline size_t range_start() const;

  inline size_t range_end() const;

  inline std::optional<std::string> reorder_line(size_t range_start, size_t range_end) const;

  inline uint8_t level_at(size_t pos) const;

  inline const icu4x::capi::BidiParagraph* AsFFI() const;
  inline icu4x::capi::BidiParagraph* AsFFI();
  inline static const icu4x::BidiParagraph* FromFFI(const icu4x::capi::BidiParagraph* ptr);
  inline static icu4x::BidiParagraph* FromFFI(icu4x::capi::BidiParagraph* ptr);
  inline static void operator delete(void* ptr);
private:
  BidiParagraph() = delete;
  BidiParagraph(const icu4x::BidiParagraph&) = delete;
  BidiParagraph(icu4x::BidiParagraph&&) noexcept = delete;
  BidiParagraph operator=(const icu4x::BidiParagraph&) = delete;
  BidiParagraph operator=(icu4x::BidiParagraph&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_BidiParagraph_D_HPP
