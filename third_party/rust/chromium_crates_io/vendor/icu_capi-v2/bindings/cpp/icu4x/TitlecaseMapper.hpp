#ifndef ICU4X_TitlecaseMapper_HPP
#define ICU4X_TitlecaseMapper_HPP

#include "TitlecaseMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Locale.hpp"
#include "TitlecaseOptionsV1.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_TitlecaseMapper_create_mv1_result {union {icu4x::capi::TitlecaseMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_TitlecaseMapper_create_mv1_result;
    icu4x_TitlecaseMapper_create_mv1_result icu4x_TitlecaseMapper_create_mv1(void);

    typedef struct icu4x_TitlecaseMapper_create_with_provider_mv1_result {union {icu4x::capi::TitlecaseMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_TitlecaseMapper_create_with_provider_mv1_result;
    icu4x_TitlecaseMapper_create_with_provider_mv1_result icu4x_TitlecaseMapper_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_TitlecaseMapper_titlecase_segment_v1_mv1(const icu4x::capi::TitlecaseMapper* self, icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::capi::TitlecaseOptionsV1 options, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_TitlecaseMapper_titlecase_segment_with_compiled_data_v1_mv1(icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::capi::TitlecaseOptionsV1 options, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_TitlecaseMapper_destroy_mv1(TitlecaseMapper* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError> icu4x::TitlecaseMapper::create() {
    auto result = icu4x::capi::icu4x_TitlecaseMapper_create_mv1();
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TitlecaseMapper>>(std::unique_ptr<icu4x::TitlecaseMapper>(icu4x::TitlecaseMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError> icu4x::TitlecaseMapper::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_TitlecaseMapper_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::TitlecaseMapper>>(std::unique_ptr<icu4x::TitlecaseMapper>(icu4x::TitlecaseMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::TitlecaseMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::TitlecaseMapper::titlecase_segment_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_TitlecaseMapper_titlecase_segment_v1_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::TitlecaseMapper::titlecase_segment_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_TitlecaseMapper_titlecase_segment_v1_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::TitlecaseMapper::titlecase_segment_with_compiled_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_TitlecaseMapper_titlecase_segment_with_compiled_data_v1_mv1({s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::TitlecaseMapper::titlecase_segment_with_compiled_data_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_TitlecaseMapper_titlecase_segment_with_compiled_data_v1_mv1({s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline const icu4x::capi::TitlecaseMapper* icu4x::TitlecaseMapper::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TitlecaseMapper*>(this);
}

inline icu4x::capi::TitlecaseMapper* icu4x::TitlecaseMapper::AsFFI() {
    return reinterpret_cast<icu4x::capi::TitlecaseMapper*>(this);
}

inline const icu4x::TitlecaseMapper* icu4x::TitlecaseMapper::FromFFI(const icu4x::capi::TitlecaseMapper* ptr) {
    return reinterpret_cast<const icu4x::TitlecaseMapper*>(ptr);
}

inline icu4x::TitlecaseMapper* icu4x::TitlecaseMapper::FromFFI(icu4x::capi::TitlecaseMapper* ptr) {
    return reinterpret_cast<icu4x::TitlecaseMapper*>(ptr);
}

inline void icu4x::TitlecaseMapper::operator delete(void* ptr) {
    icu4x::capi::icu4x_TitlecaseMapper_destroy_mv1(reinterpret_cast<icu4x::capi::TitlecaseMapper*>(ptr));
}


#endif // ICU4X_TitlecaseMapper_HPP
