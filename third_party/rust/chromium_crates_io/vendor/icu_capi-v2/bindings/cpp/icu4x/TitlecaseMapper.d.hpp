#ifndef ICU4X_TitlecaseMapper_D_HPP
#define ICU4X_TitlecaseMapper_D_HPP

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
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
namespace capi { struct TitlecaseMapper; }
class TitlecaseMapper;
struct TitlecaseOptionsV1;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct TitlecaseMapper;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `TitlecaseMapper`](https://docs.rs/icu/2.2.0/icu/casemap/struct.TitlecaseMapper.html) for more information.
 */
class TitlecaseMapper {
public:

  /**
   * Construct a new `TitlecaseMapper` instance using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/casemap/struct.TitlecaseMapper.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError> create();

  /**
   * Construct a new `TitlecaseMapper` instance using a particular data source.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/casemap/struct.TitlecaseMapper.html#method.new) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  /**
   * Returns the full titlecase mapping of the given string
   *
   * The `v1` refers to the version of the options struct, which may change as we add more options
   *
   * See the [Rust documentation for `titlecase_segment`](https://docs.rs/icu/2.2.0/icu/casemap/struct.TitlecaseMapperBorrowed.html#method.titlecase_segment) for more information.
   */
  inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> titlecase_segment_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const;
  template<typename W>
  inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> titlecase_segment_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable_output) const;

  /**
   * Returns the full titlecase mapping of the given string, using compiled data (avoids having to allocate a `TitlecaseMapper` object)
   *
   * The `v1` refers to the version of the options struct, which may change as we add more options
   *
   * See the [Rust documentation for `titlecase_segment`](https://docs.rs/icu/2.2.0/icu/casemap/struct.TitlecaseMapperBorrowed.html#method.titlecase_segment) for more information.
   */
  inline static icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> titlecase_segment_with_compiled_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options);
  template<typename W>
  inline static icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> titlecase_segment_with_compiled_data_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable_output);

    inline const icu4x::capi::TitlecaseMapper* AsFFI() const;
    inline icu4x::capi::TitlecaseMapper* AsFFI();
    inline static const icu4x::TitlecaseMapper* FromFFI(const icu4x::capi::TitlecaseMapper* ptr);
    inline static icu4x::TitlecaseMapper* FromFFI(icu4x::capi::TitlecaseMapper* ptr);
    inline static void operator delete(void* ptr);
private:
    TitlecaseMapper() = delete;
    TitlecaseMapper(const icu4x::TitlecaseMapper&) = delete;
    TitlecaseMapper(icu4x::TitlecaseMapper&&) noexcept = delete;
    TitlecaseMapper operator=(const icu4x::TitlecaseMapper&) = delete;
    TitlecaseMapper operator=(icu4x::TitlecaseMapper&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_TitlecaseMapper_D_HPP
