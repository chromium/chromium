#ifndef ICU4X_Locale_HPP
#define ICU4X_Locale_HPP

#include "Locale.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "LocaleParseError.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_Locale_from_string_mv1_result {union {icu4x::capi::Locale* ok; icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_from_string_mv1_result;
    icu4x_Locale_from_string_mv1_result icu4x_Locale_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView name);

    icu4x::capi::Locale* icu4x_Locale_unknown_mv1(void);

    const icu4x::capi::Locale* icu4x_Locale_unknown_ref_mv1(void);

    icu4x::capi::Locale* icu4x_Locale_clone_mv1(const icu4x::capi::Locale* self);

    void icu4x_Locale_basename_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_Locale_get_unicode_extension_mv1_result { bool is_ok;} icu4x_Locale_get_unicode_extension_mv1_result;
    icu4x_Locale_get_unicode_extension_mv1_result icu4x_Locale_get_unicode_extension_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_Locale_set_unicode_extension_mv1_result { bool is_ok;} icu4x_Locale_set_unicode_extension_mv1_result;
    icu4x_Locale_set_unicode_extension_mv1_result icu4x_Locale_set_unicode_extension_mv1(icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView k, icu4x::diplomat::capi::DiplomatStringView v);

    void icu4x_Locale_language_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_Locale_set_language_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_set_language_mv1_result;
    icu4x_Locale_set_language_mv1_result icu4x_Locale_set_language_mv1(icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s);

    typedef struct icu4x_Locale_region_mv1_result { bool is_ok;} icu4x_Locale_region_mv1_result;
    icu4x_Locale_region_mv1_result icu4x_Locale_region_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_Locale_set_region_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_set_region_mv1_result;
    icu4x_Locale_set_region_mv1_result icu4x_Locale_set_region_mv1(icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s);

    typedef struct icu4x_Locale_script_mv1_result { bool is_ok;} icu4x_Locale_script_mv1_result;
    icu4x_Locale_script_mv1_result icu4x_Locale_script_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatWrite* write);

    typedef struct icu4x_Locale_set_script_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_set_script_mv1_result;
    icu4x_Locale_set_script_mv1_result icu4x_Locale_set_script_mv1(icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s);

    void icu4x_Locale_variants_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatWrite* write);

    size_t icu4x_Locale_variant_count_mv1(const icu4x::capi::Locale* self);

    typedef struct icu4x_Locale_variant_at_mv1_result { bool is_ok;} icu4x_Locale_variant_at_mv1_result;
    icu4x_Locale_variant_at_mv1_result icu4x_Locale_variant_at_mv1(const icu4x::capi::Locale* self, size_t index, icu4x::diplomat::capi::DiplomatWrite* write);

    bool icu4x_Locale_has_variant_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s);

    typedef struct icu4x_Locale_add_variant_mv1_result {union {bool ok; icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_add_variant_mv1_result;
    icu4x_Locale_add_variant_mv1_result icu4x_Locale_add_variant_mv1(icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s);

    bool icu4x_Locale_remove_variant_mv1(icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView s);

    void icu4x_Locale_clear_variants_mv1(icu4x::capi::Locale* self);

    typedef struct icu4x_Locale_normalize_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_normalize_mv1_result;
    icu4x_Locale_normalize_mv1_result icu4x_Locale_normalize_mv1(icu4x::diplomat::capi::DiplomatStringView s, icu4x::diplomat::capi::DiplomatWrite* write);

    void icu4x_Locale_to_string_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatWrite* write);

    bool icu4x_Locale_normalizing_eq_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView other);

    int8_t icu4x_Locale_compare_to_string_mv1(const icu4x::capi::Locale* self, icu4x::diplomat::capi::DiplomatStringView other);

    int8_t icu4x_Locale_compare_to_mv1(const icu4x::capi::Locale* self, const icu4x::capi::Locale* other);

    void icu4x_Locale_destroy_mv1(Locale* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError> icu4x::Locale::from_string(std::string_view name) {
    auto result = icu4x::capi::icu4x_Locale_from_string_mv1({name.data(), name.size()});
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Locale>>(std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Locale> icu4x::Locale::unknown() {
    auto result = icu4x::capi::icu4x_Locale_unknown_mv1();
    return std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result));
}

inline const icu4x::Locale& icu4x::Locale::unknown_ref() {
    auto result = icu4x::capi::icu4x_Locale_unknown_ref_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline std::unique_ptr<icu4x::Locale> icu4x::Locale::clone() const {
    auto result = icu4x::capi::icu4x_Locale_clone_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result));
}

inline std::string icu4x::Locale::basename() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Locale_basename_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Locale::basename_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Locale_basename_mv1(this->AsFFI(),
        &write);
}

inline std::optional<std::string> icu4x::Locale::get_unicode_extension(std::string_view s) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_Locale_get_unicode_extension_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}
template<typename W>
inline std::optional<std::monostate> icu4x::Locale::get_unicode_extension_write(std::string_view s, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_Locale_get_unicode_extension_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return result.is_ok ? std::optional<std::monostate>() : std::nullopt;
}

inline std::optional<std::monostate> icu4x::Locale::set_unicode_extension(std::string_view k, std::string_view v) {
    auto result = icu4x::capi::icu4x_Locale_set_unicode_extension_mv1(this->AsFFI(),
        {k.data(), k.size()},
        {v.data(), v.size()});
    return result.is_ok ? std::optional<std::monostate>() : std::nullopt;
}

inline std::string icu4x::Locale::language() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Locale_language_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Locale::language_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Locale_language_mv1(this->AsFFI(),
        &write);
}

inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::set_language(std::string_view s) {
    auto result = icu4x::capi::icu4x_Locale_set_language_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::optional<std::string> icu4x::Locale::region() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_Locale_region_mv1(this->AsFFI(),
        &write);
    return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}
template<typename W>
inline std::optional<std::monostate> icu4x::Locale::region_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_Locale_region_mv1(this->AsFFI(),
        &write);
    return result.is_ok ? std::optional<std::monostate>() : std::nullopt;
}

inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::set_region(std::string_view s) {
    auto result = icu4x::capi::icu4x_Locale_set_region_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::optional<std::string> icu4x::Locale::script() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_Locale_script_mv1(this->AsFFI(),
        &write);
    return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}
template<typename W>
inline std::optional<std::monostate> icu4x::Locale::script_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_Locale_script_mv1(this->AsFFI(),
        &write);
    return result.is_ok ? std::optional<std::monostate>() : std::nullopt;
}

inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::set_script(std::string_view s) {
    auto result = icu4x::capi::icu4x_Locale_set_script_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::string icu4x::Locale::variants() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Locale_variants_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Locale::variants_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Locale_variants_mv1(this->AsFFI(),
        &write);
}

inline size_t icu4x::Locale::variant_count() const {
    auto result = icu4x::capi::icu4x_Locale_variant_count_mv1(this->AsFFI());
    return result;
}

inline std::optional<std::string> icu4x::Locale::variant_at(size_t index) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_Locale_variant_at_mv1(this->AsFFI(),
        index,
        &write);
    return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}
template<typename W>
inline std::optional<std::monostate> icu4x::Locale::variant_at_write(size_t index, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_Locale_variant_at_mv1(this->AsFFI(),
        index,
        &write);
    return result.is_ok ? std::optional<std::monostate>() : std::nullopt;
}

inline bool icu4x::Locale::has_variant(std::string_view s) const {
    auto result = icu4x::capi::icu4x_Locale_has_variant_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline icu4x::diplomat::result<bool, icu4x::LocaleParseError> icu4x::Locale::add_variant(std::string_view s) {
    auto result = icu4x::capi::icu4x_Locale_add_variant_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result.is_ok ? icu4x::diplomat::result<bool, icu4x::LocaleParseError>(icu4x::diplomat::Ok<bool>(result.ok)) : icu4x::diplomat::result<bool, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline bool icu4x::Locale::remove_variant(std::string_view s) {
    auto result = icu4x::capi::icu4x_Locale_remove_variant_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline void icu4x::Locale::clear_variants() {
    icu4x::capi::icu4x_Locale_clear_variants_mv1(this->AsFFI());
}

inline icu4x::diplomat::result<std::string, icu4x::LocaleParseError> icu4x::Locale::normalize(std::string_view s) {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_Locale_normalize_mv1({s.data(), s.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::string, icu4x::LocaleParseError>(icu4x::diplomat::Ok<std::string>(std::move(output))) : icu4x::diplomat::result<std::string, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}
template<typename W>
inline icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::normalize_write(std::string_view s, W& writeable) {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_Locale_normalize_mv1({s.data(), s.size()},
        &write);
    return result.is_ok ? icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Ok<std::monostate>()) : icu4x::diplomat::result<std::monostate, icu4x::LocaleParseError>(icu4x::diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::string icu4x::Locale::to_string() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Locale_to_string_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Locale::to_string_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Locale_to_string_mv1(this->AsFFI(),
        &write);
}

inline bool icu4x::Locale::normalizing_eq(std::string_view other) const {
    auto result = icu4x::capi::icu4x_Locale_normalizing_eq_mv1(this->AsFFI(),
        {other.data(), other.size()});
    return result;
}

inline int8_t icu4x::Locale::compare_to_string(std::string_view other) const {
    auto result = icu4x::capi::icu4x_Locale_compare_to_string_mv1(this->AsFFI(),
        {other.data(), other.size()});
    return result;
}

inline int8_t icu4x::Locale::compare_to(const icu4x::Locale& other) const {
    auto result = icu4x::capi::icu4x_Locale_compare_to_mv1(this->AsFFI(),
        other.AsFFI());
    return result;
}
inline bool icu4x::Locale::operator==(const icu4x::Locale& other) const {
    return this->compare_to(other) == 0;
}

inline bool icu4x::Locale::operator!=(const icu4x::Locale& other) const {
    return this->compare_to(other) != 0;
}

inline bool icu4x::Locale::operator<=(const icu4x::Locale& other) const {
    return this->compare_to(other) <= 0;
}

inline bool icu4x::Locale::operator>=(const icu4x::Locale& other) const {
    return this->compare_to(other) >= 0;
}

inline bool icu4x::Locale::operator<(const icu4x::Locale& other) const {
    return this->compare_to(other) < 0;
}

inline bool icu4x::Locale::operator>(const icu4x::Locale& other) const {
    return this->compare_to(other) > 0;
}

inline const icu4x::capi::Locale* icu4x::Locale::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Locale*>(this);
}

inline icu4x::capi::Locale* icu4x::Locale::AsFFI() {
    return reinterpret_cast<icu4x::capi::Locale*>(this);
}

inline const icu4x::Locale* icu4x::Locale::FromFFI(const icu4x::capi::Locale* ptr) {
    return reinterpret_cast<const icu4x::Locale*>(ptr);
}

inline icu4x::Locale* icu4x::Locale::FromFFI(icu4x::capi::Locale* ptr) {
    return reinterpret_cast<icu4x::Locale*>(ptr);
}

inline void icu4x::Locale::operator delete(void* ptr) {
    icu4x::capi::icu4x_Locale_destroy_mv1(reinterpret_cast<icu4x::capi::Locale*>(ptr));
}


#endif // ICU4X_Locale_HPP
