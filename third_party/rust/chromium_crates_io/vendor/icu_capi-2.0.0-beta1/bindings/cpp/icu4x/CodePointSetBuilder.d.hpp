#ifndef icu4x_CodePointSetBuilder_D_HPP
#define icu4x_CodePointSetBuilder_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CodePointSetBuilder; }
class CodePointSetBuilder;
namespace capi { struct CodePointSetData; }
class CodePointSetData;
}


namespace icu4x {
namespace capi {
    struct CodePointSetBuilder;
} // namespace capi
} // namespace

namespace icu4x {
class CodePointSetBuilder {
public:

  inline static std::unique_ptr<icu4x::CodePointSetBuilder> create();

  inline std::unique_ptr<icu4x::CodePointSetData> build();

  inline void complement();

  inline bool is_empty() const;

  inline void add_char(char32_t ch);

  inline void add_inclusive_range(char32_t start, char32_t end);

  inline void add_set(const icu4x::CodePointSetData& data);

  inline void remove_char(char32_t ch);

  inline void remove_inclusive_range(char32_t start, char32_t end);

  inline void remove_set(const icu4x::CodePointSetData& data);

  inline void retain_char(char32_t ch);

  inline void retain_inclusive_range(char32_t start, char32_t end);

  inline void retain_set(const icu4x::CodePointSetData& data);

  inline void complement_char(char32_t ch);

  inline void complement_inclusive_range(char32_t start, char32_t end);

  inline void complement_set(const icu4x::CodePointSetData& data);

  inline const icu4x::capi::CodePointSetBuilder* AsFFI() const;
  inline icu4x::capi::CodePointSetBuilder* AsFFI();
  inline static const icu4x::CodePointSetBuilder* FromFFI(const icu4x::capi::CodePointSetBuilder* ptr);
  inline static icu4x::CodePointSetBuilder* FromFFI(icu4x::capi::CodePointSetBuilder* ptr);
  inline static void operator delete(void* ptr);
private:
  CodePointSetBuilder() = delete;
  CodePointSetBuilder(const icu4x::CodePointSetBuilder&) = delete;
  CodePointSetBuilder(icu4x::CodePointSetBuilder&&) noexcept = delete;
  CodePointSetBuilder operator=(const icu4x::CodePointSetBuilder&) = delete;
  CodePointSetBuilder operator=(icu4x::CodePointSetBuilder&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CodePointSetBuilder_D_HPP
