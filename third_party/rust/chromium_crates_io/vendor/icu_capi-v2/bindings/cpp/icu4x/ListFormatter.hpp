#ifndef ICU4X_ListFormatter_HPP
#define ICU4X_ListFormatter_HPP

#include "ListFormatter.d.hpp"

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
#include "ListLength.hpp"
#include "Locale.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_ListFormatter_create_and_with_length_mv1_result {union {icu4x::capi::ListFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ListFormatter_create_and_with_length_mv1_result;
    icu4x_ListFormatter_create_and_with_length_mv1_result icu4x_ListFormatter_create_and_with_length_mv1(const icu4x::capi::Locale* locale, icu4x::capi::ListLength length);

    typedef struct icu4x_ListFormatter_create_and_with_length_and_provider_mv1_result {union {icu4x::capi::ListFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ListFormatter_create_and_with_length_and_provider_mv1_result;
    icu4x_ListFormatter_create_and_with_length_and_provider_mv1_result icu4x_ListFormatter_create_and_with_length_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::ListLength length);

    typedef struct icu4x_ListFormatter_create_or_with_length_mv1_result {union {icu4x::capi::ListFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ListFormatter_create_or_with_length_mv1_result;
    icu4x_ListFormatter_create_or_with_length_mv1_result icu4x_ListFormatter_create_or_with_length_mv1(const icu4x::capi::Locale* locale, icu4x::capi::ListLength length);

    typedef struct icu4x_ListFormatter_create_or_with_length_and_provider_mv1_result {union {icu4x::capi::ListFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ListFormatter_create_or_with_length_and_provider_mv1_result;
    icu4x_ListFormatter_create_or_with_length_and_provider_mv1_result icu4x_ListFormatter_create_or_with_length_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::ListLength length);

    typedef struct icu4x_ListFormatter_create_unit_with_length_mv1_result {union {icu4x::capi::ListFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ListFormatter_create_unit_with_length_mv1_result;
    icu4x_ListFormatter_create_unit_with_length_mv1_result icu4x_ListFormatter_create_unit_with_length_mv1(const icu4x::capi::Locale* locale, icu4x::capi::ListLength length);

    typedef struct icu4x_ListFormatter_create_unit_with_length_and_provider_mv1_result {union {icu4x::capi::ListFormatter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ListFormatter_create_unit_with_length_and_provider_mv1_result;
    icu4x_ListFormatter_create_unit_with_length_and_provider_mv1_result icu4x_ListFormatter_create_unit_with_length_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale, icu4x::capi::ListLength length);

    void icu4x_ListFormatter_format_utf8_mv1(const icu4x::capi::ListFormatter* self, icu4x::diplomat::capi::DiplomatStringsView list, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_ListFormatter_format_utf16_mv1(const icu4x::capi::ListFormatter* self, icu4x::diplomat::capi::DiplomatStrings16View list, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_ListFormatter_destroy_mv1(ListFormatter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> icu4x::ListFormatter::create_and_with_length(const icu4x::Locale& locale, icu4x::ListLength length) {
    auto result = icu4x::capi::icu4x_ListFormatter_create_and_with_length_mv1(locale.AsFFI(),
        length.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ListFormatter>>(std::unique_ptr<icu4x::ListFormatter>(icu4x::ListFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> icu4x::ListFormatter::create_and_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length) {
    auto result = icu4x::capi::icu4x_ListFormatter_create_and_with_length_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ListFormatter>>(std::unique_ptr<icu4x::ListFormatter>(icu4x::ListFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> icu4x::ListFormatter::create_or_with_length(const icu4x::Locale& locale, icu4x::ListLength length) {
    auto result = icu4x::capi::icu4x_ListFormatter_create_or_with_length_mv1(locale.AsFFI(),
        length.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ListFormatter>>(std::unique_ptr<icu4x::ListFormatter>(icu4x::ListFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> icu4x::ListFormatter::create_or_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length) {
    auto result = icu4x::capi::icu4x_ListFormatter_create_or_with_length_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ListFormatter>>(std::unique_ptr<icu4x::ListFormatter>(icu4x::ListFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> icu4x::ListFormatter::create_unit_with_length(const icu4x::Locale& locale, icu4x::ListLength length) {
    auto result = icu4x::capi::icu4x_ListFormatter_create_unit_with_length_mv1(locale.AsFFI(),
        length.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ListFormatter>>(std::unique_ptr<icu4x::ListFormatter>(icu4x::ListFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError> icu4x::ListFormatter::create_unit_with_length_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::ListLength length) {
    auto result = icu4x::capi::icu4x_ListFormatter_create_unit_with_length_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI(),
        length.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ListFormatter>>(std::unique_ptr<icu4x::ListFormatter>(icu4x::ListFormatter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ListFormatter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::ListFormatter::format(icu4x::diplomat::span<const diplomat::string_view_for_slice> list) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_ListFormatter_format_utf8_mv1(this->AsFFI(),
        {reinterpret_cast<const icu4x::diplomat::capi::DiplomatStringView*>(list.data()), list.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::ListFormatter::format_write(icu4x::diplomat::span<const diplomat::string_view_for_slice> list, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_ListFormatter_format_utf8_mv1(this->AsFFI(),
        {reinterpret_cast<const icu4x::diplomat::capi::DiplomatStringView*>(list.data()), list.size()},
        &write);
}

inline std::string icu4x::ListFormatter::format16(icu4x::diplomat::span<const diplomat::u16string_view_for_slice> list) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_ListFormatter_format_utf16_mv1(this->AsFFI(),
        {reinterpret_cast<const icu4x::diplomat::capi::DiplomatStringView*>(list.data()), list.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::ListFormatter::format16_write(icu4x::diplomat::span<const diplomat::u16string_view_for_slice> list, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_ListFormatter_format_utf16_mv1(this->AsFFI(),
        {reinterpret_cast<const icu4x::diplomat::capi::DiplomatStringView*>(list.data()), list.size()},
        &write);
}

inline const icu4x::capi::ListFormatter* icu4x::ListFormatter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ListFormatter*>(this);
}

inline icu4x::capi::ListFormatter* icu4x::ListFormatter::AsFFI() {
    return reinterpret_cast<icu4x::capi::ListFormatter*>(this);
}

inline const icu4x::ListFormatter* icu4x::ListFormatter::FromFFI(const icu4x::capi::ListFormatter* ptr) {
    return reinterpret_cast<const icu4x::ListFormatter*>(ptr);
}

inline icu4x::ListFormatter* icu4x::ListFormatter::FromFFI(icu4x::capi::ListFormatter* ptr) {
    return reinterpret_cast<icu4x::ListFormatter*>(ptr);
}

inline void icu4x::ListFormatter::operator delete(void* ptr) {
    icu4x::capi::icu4x_ListFormatter_destroy_mv1(reinterpret_cast<icu4x::capi::ListFormatter*>(ptr));
}


#endif // ICU4X_ListFormatter_HPP
