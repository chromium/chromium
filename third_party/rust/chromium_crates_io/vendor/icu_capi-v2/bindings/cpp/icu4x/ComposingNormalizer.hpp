#ifndef ICU4X_ComposingNormalizer_HPP
#define ICU4X_ComposingNormalizer_HPP

#include "ComposingNormalizer.d.hpp"

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
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::ComposingNormalizer* icu4x_ComposingNormalizer_create_nfc_mv1(void);

    typedef struct icu4x_ComposingNormalizer_create_nfc_with_provider_mv1_result {union {icu4x::capi::ComposingNormalizer* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ComposingNormalizer_create_nfc_with_provider_mv1_result;
    icu4x_ComposingNormalizer_create_nfc_with_provider_mv1_result icu4x_ComposingNormalizer_create_nfc_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::ComposingNormalizer* icu4x_ComposingNormalizer_create_nfkc_mv1(void);

    typedef struct icu4x_ComposingNormalizer_create_nfkc_with_provider_mv1_result {union {icu4x::capi::ComposingNormalizer* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ComposingNormalizer_create_nfkc_with_provider_mv1_result;
    icu4x_ComposingNormalizer_create_nfkc_with_provider_mv1_result icu4x_ComposingNormalizer_create_nfkc_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_ComposingNormalizer_normalize_mv1(const icu4x::capi::ComposingNormalizer* self, icu4x::diplomat::capi::DiplomatStringView s, icu4x::diplomat::capi::DiplomatWrite* write);

    bool icu4x_ComposingNormalizer_is_normalized_utf8_mv1(const icu4x::capi::ComposingNormalizer* self, icu4x::diplomat::capi::DiplomatStringView s);

    bool icu4x_ComposingNormalizer_is_normalized_utf16_mv1(const icu4x::capi::ComposingNormalizer* self, icu4x::diplomat::capi::DiplomatString16View s);

    size_t icu4x_ComposingNormalizer_is_normalized_utf8_up_to_mv1(const icu4x::capi::ComposingNormalizer* self, icu4x::diplomat::capi::DiplomatStringView s);

    size_t icu4x_ComposingNormalizer_is_normalized_utf16_up_to_mv1(const icu4x::capi::ComposingNormalizer* self, icu4x::diplomat::capi::DiplomatString16View s);

    void icu4x_ComposingNormalizer_destroy_mv1(ComposingNormalizer* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::ComposingNormalizer> icu4x::ComposingNormalizer::create_nfc() {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_create_nfc_mv1();
    return std::unique_ptr<icu4x::ComposingNormalizer>(icu4x::ComposingNormalizer::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError> icu4x::ComposingNormalizer::create_nfc_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_create_nfc_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ComposingNormalizer>>(std::unique_ptr<icu4x::ComposingNormalizer>(icu4x::ComposingNormalizer::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::ComposingNormalizer> icu4x::ComposingNormalizer::create_nfkc() {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_create_nfkc_mv1();
    return std::unique_ptr<icu4x::ComposingNormalizer>(icu4x::ComposingNormalizer::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError> icu4x::ComposingNormalizer::create_nfkc_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_create_nfkc_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ComposingNormalizer>>(std::unique_ptr<icu4x::ComposingNormalizer>(icu4x::ComposingNormalizer::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::string icu4x::ComposingNormalizer::normalize(std::string_view s) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_ComposingNormalizer_normalize_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
    return output;
}
template<typename W>
inline void icu4x::ComposingNormalizer::normalize_write(std::string_view s, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_ComposingNormalizer_normalize_mv1(this->AsFFI(),
        {s.data(), s.size()},
        &write);
}

inline bool icu4x::ComposingNormalizer::is_normalized(std::string_view s) const {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_is_normalized_utf8_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline bool icu4x::ComposingNormalizer::is_normalized16(std::u16string_view s) const {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_is_normalized_utf16_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline size_t icu4x::ComposingNormalizer::is_normalized_up_to(std::string_view s) const {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_is_normalized_utf8_up_to_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline size_t icu4x::ComposingNormalizer::is_normalized16_up_to(std::u16string_view s) const {
    auto result = icu4x::capi::icu4x_ComposingNormalizer_is_normalized_utf16_up_to_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline const icu4x::capi::ComposingNormalizer* icu4x::ComposingNormalizer::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ComposingNormalizer*>(this);
}

inline icu4x::capi::ComposingNormalizer* icu4x::ComposingNormalizer::AsFFI() {
    return reinterpret_cast<icu4x::capi::ComposingNormalizer*>(this);
}

inline const icu4x::ComposingNormalizer* icu4x::ComposingNormalizer::FromFFI(const icu4x::capi::ComposingNormalizer* ptr) {
    return reinterpret_cast<const icu4x::ComposingNormalizer*>(ptr);
}

inline icu4x::ComposingNormalizer* icu4x::ComposingNormalizer::FromFFI(icu4x::capi::ComposingNormalizer* ptr) {
    return reinterpret_cast<icu4x::ComposingNormalizer*>(ptr);
}

inline void icu4x::ComposingNormalizer::operator delete(void* ptr) {
    icu4x::capi::icu4x_ComposingNormalizer_destroy_mv1(reinterpret_cast<icu4x::capi::ComposingNormalizer*>(ptr));
}


#endif // ICU4X_ComposingNormalizer_HPP
