#ifndef ICU4X_CaseMapCloser_D_HPP
#define ICU4X_CaseMapCloser_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct CaseMapCloser; }
class CaseMapCloser;
namespace capi { struct CodePointSetBuilder; }
class CodePointSetBuilder;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct CaseMapCloser;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `CaseMapCloser`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapCloser.html) for more information.
 */
class CaseMapCloser {
public:

  /**
   * Construct a new `CaseMapCloser` instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapCloser.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError> create();

  /**
   * Construct a new `CaseMapCloser` instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapCloser.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapCloser>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Adds all simple case mappings and the full case folding for `c` to `builder`.
   * Also adds special case closure mappings.
   *
   * See the [Rust documentation for `add_case_closure_to`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapCloserBorrowed.html#method.add_case_closure_to) for more information.
   */
  inline void add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const;

  /**
   * Finds all characters and strings which may casemap to `s` as their full case folding string
   * and adds them to the set.
   *
   * Returns true if the string was found
   *
   * See the [Rust documentation for `add_string_case_closure_to`](https://docs.rs/icu/2.2.0/icu/casemap/struct.CaseMapCloserBorrowed.html#method.add_string_case_closure_to) for more information.
   */
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
#endif // ICU4X_CaseMapCloser_D_HPP
