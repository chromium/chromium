#ifndef ICU4X_CaseMapper_HPP
#define ICU4X_CaseMapper_HPP

#include "CaseMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointSetBuilder.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Locale.hpp"
#include "TitlecaseOptionsV1.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::CaseMapper* icu4x_CaseMapper_create_mv1(void);

    typedef struct icu4x_CaseMapper_create_with_provider_mv1_result {union {icu4x::capi::CaseMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CaseMapper_create_with_provider_mv1_result;
    icu4x_CaseMapper_create_with_provider_mv1_result icu4x_CaseMapper_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_CaseMapper_lowercase_mv1(const icu4x::capi::CaseMapper* self, icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_uppercase_mv1(const icu4x::capi::CaseMapper* self, icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_lowercase_with_compiled_data_mv1(icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_uppercase_with_compiled_data_mv1(icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_titlecase_segment_with_only_case_data_v1_mv1(const icu4x::capi::CaseMapper* self, icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::capi::TitlecaseOptionsV1 options, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_titlecase_segment_with_only_case_compiled_data_v1_mv1(icu4x::diplomat::capi::DiplomatStringView s, const icu4x::capi::Locale* locale, icu4x::capi::TitlecaseOptionsV1 options, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_fold_mv1(const icu4x::capi::CaseMapper* self, icu4x::diplomat::capi::DiplomatStringView s, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_fold_turkic_mv1(const icu4x::capi::CaseMapper* self, icu4x::diplomat::capi::DiplomatStringView s, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_CaseMapper_add_case_closure_to_mv1(const icu4x::capi::CaseMapper* self, char32_t c, icu4x::capi::CodePointSetBuilder* builder);

    char32_t icu4x_CaseMapper_simple_lowercase_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);

    char32_t icu4x_CaseMapper_simple_lowercase_with_compiled_data_mv1(char32_t ch);

    char32_t icu4x_CaseMapper_simple_uppercase_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);

    char32_t icu4x_CaseMapper_simple_uppercase_with_compiled_data_mv1(char32_t ch);

    char32_t icu4x_CaseMapper_simple_titlecase_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);

    char32_t icu4x_CaseMapper_simple_titlecase_with_compiled_data_mv1(char32_t ch);

    char32_t icu4x_CaseMapper_simple_fold_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);

    char32_t icu4x_CaseMapper_simple_fold_with_compiled_data_mv1(char32_t ch);

    char32_t icu4x_CaseMapper_simple_fold_turkic_mv1(const icu4x::capi::CaseMapper* self, char32_t ch);

    char32_t icu4x_CaseMapper_simple_fold_turkic_with_compiled_data_mv1(char32_t ch);

    void icu4x_CaseMapper_destroy_mv1(CaseMapper* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::CaseMapper> icu4x::CaseMapper::create() {
    auto result = icu4x::capi::icu4x_CaseMapper_create_mv1();
    return std::unique_ptr<icu4x::CaseMapper>(icu4x::CaseMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError> icu4x::CaseMapper::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CaseMapper_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CaseMapper>>(std::unique_ptr<icu4x::CaseMapper>(icu4x::CaseMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CaseMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::lowercase(std::string_view s, const icu4x::Locale& locale) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_lowercase_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::lowercase_write(std::string_view s, const icu4x::Locale& locale, W& writeable) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_lowercase_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::uppercase(std::string_view s, const icu4x::Locale& locale) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_uppercase_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::uppercase_write(std::string_view s, const icu4x::Locale& locale, W& writeable) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_uppercase_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::lowercase_with_compiled_data(std::string_view s, const icu4x::Locale& locale) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_lowercase_with_compiled_data_mv1({s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::lowercase_with_compiled_data_write(std::string_view s, const icu4x::Locale& locale, W& writeable) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_lowercase_with_compiled_data_mv1({s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::uppercase_with_compiled_data(std::string_view s, const icu4x::Locale& locale) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_uppercase_with_compiled_data_mv1({s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::uppercase_with_compiled_data_write(std::string_view s, const icu4x::Locale& locale, W& writeable) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_uppercase_with_compiled_data_mv1({s.data(), s.size()},
        locale.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::titlecase_segment_with_only_case_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_titlecase_segment_with_only_case_data_v1_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::titlecase_segment_with_only_case_data_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_titlecase_segment_with_only_case_data_v1_mv1(this->AsFFI(),
        {s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::titlecase_segment_with_only_case_compiled_data_v1(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_titlecase_segment_with_only_case_compiled_data_v1_mv1({s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::titlecase_segment_with_only_case_compiled_data_v1_write(std::string_view s, const icu4x::Locale& locale, icu4x::TitlecaseOptionsV1 options, W& writeable) {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_titlecase_segment_with_only_case_compiled_data_v1_mv1({s.data(), s.size()},
        locale.AsFFI(),
        options.AsFFI(),
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::fold(std::string_view s) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_fold_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::fold_write(std::string_view s, W& writeable) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_fold_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline icu4x::diplomat::result<std::string, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::fold_turkic(std::string_view s) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_CaseMapper_fold_turkic_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return icu4x::diplomat::Ok<std::string>(std::move(output));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::diplomat::Utf8Error> icu4x::CaseMapper::fold_turkic_write(std::string_view s, W& writeable) const {
    if (!icu4x::diplomat::capi::diplomat_is_str(s.data(), s.size())) {
    return icu4x::diplomat::Err<icu4x::diplomat::Utf8Error>();
  }
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_CaseMapper_fold_turkic_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return icu4x::diplomat::Ok<std::monostate>();
}

inline void icu4x::CaseMapper::add_case_closure_to(char32_t c, icu4x::CodePointSetBuilder& builder) const {
    icu4x::capi::icu4x_CaseMapper_add_case_closure_to_mv1(this->AsFFI(),
        c,
        builder.AsFFI());
}

inline char32_t icu4x::CaseMapper::simple_lowercase(char32_t ch) const {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_lowercase_mv1(this->AsFFI(),
        ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_lowercase_with_compiled_data(char32_t ch) {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_lowercase_with_compiled_data_mv1(ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_uppercase(char32_t ch) const {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_uppercase_mv1(this->AsFFI(),
        ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_uppercase_with_compiled_data(char32_t ch) {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_uppercase_with_compiled_data_mv1(ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_titlecase(char32_t ch) const {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_titlecase_mv1(this->AsFFI(),
        ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_titlecase_with_compiled_data(char32_t ch) {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_titlecase_with_compiled_data_mv1(ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_fold(char32_t ch) const {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_fold_mv1(this->AsFFI(),
        ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_fold_with_compiled_data(char32_t ch) {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_fold_with_compiled_data_mv1(ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_fold_turkic(char32_t ch) const {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_fold_turkic_mv1(this->AsFFI(),
        ch);
    return result;
}

inline char32_t icu4x::CaseMapper::simple_fold_turkic_with_compiled_data(char32_t ch) {
    auto result = icu4x::capi::icu4x_CaseMapper_simple_fold_turkic_with_compiled_data_mv1(ch);
    return result;
}

inline const icu4x::capi::CaseMapper* icu4x::CaseMapper::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CaseMapper*>(this);
}

inline icu4x::capi::CaseMapper* icu4x::CaseMapper::AsFFI() {
    return reinterpret_cast<icu4x::capi::CaseMapper*>(this);
}

inline const icu4x::CaseMapper* icu4x::CaseMapper::FromFFI(const icu4x::capi::CaseMapper* ptr) {
    return reinterpret_cast<const icu4x::CaseMapper*>(ptr);
}

inline icu4x::CaseMapper* icu4x::CaseMapper::FromFFI(icu4x::capi::CaseMapper* ptr) {
    return reinterpret_cast<icu4x::CaseMapper*>(ptr);
}

inline void icu4x::CaseMapper::operator delete(void* ptr) {
    icu4x::capi::icu4x_CaseMapper_destroy_mv1(reinterpret_cast<icu4x::capi::CaseMapper*>(ptr));
}


#endif // ICU4X_CaseMapper_HPP
