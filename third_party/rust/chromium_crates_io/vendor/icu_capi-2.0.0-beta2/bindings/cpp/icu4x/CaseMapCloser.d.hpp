#ifndef icu4x_CaseMapCloser_D_HPP
#define icu4x_CaseMapCloser_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct CaseMapCloser; }
class CaseMapCloser;
namespace capi { struct CodePointSetBuilder; }
class CodePointSetBuilder;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
}


namespace icu4x {
namespace capi {
    struct CaseMapCloser;
} // namespace capi
} // namespace

namespace icu4x {
class CaseMapCloser {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError> create();

  inline static diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline void add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const;

  inline bool add_string_case_closure_to(std::string_view s, icu4x::CodePointSetBuilder& builder) const;

  inline const icu4x::capi::CaseMapCloser* AsFFI() const;
  inline icu4x::capi::CaseMapCloser* AsFFI();
  inline static const icu4x::CaseMapCloser* FromFFI(const icu4x::capi::CaseMapCloser* ptr);
  inline static icu4x::CaseMapCloser* FromFFI(icu4x::capi::CaseMapCloser* ptr);
  inline static void operator delete(void* ptr);
private:
  CaseMapCloser() = delete;
  CaseMapCloser(const icu4x::CaseMapCloser&) = delete;
  CaseMapCloser(icu4x::CaseMapCloser&&) noexcept = delete;
  CaseMapCloser operator=(const icu4x::CaseMapCloser&) = delete;
  CaseMapCloser operator=(icu4x::CaseMapCloser&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_CaseMapCloser_D_HPP
