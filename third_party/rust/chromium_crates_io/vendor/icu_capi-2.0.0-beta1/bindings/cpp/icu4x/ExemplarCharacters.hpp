#ifndef icu4x_ExemplarCharacters_HPP
#define icu4x_ExemplarCharacters_HPP

#include "ExemplarCharacters.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Locale.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    bool icu4x_ExemplarCharacters_contains_str_mv1(const icu4x::capi::ExemplarCharacters* self, diplomat::capi::DiplomatStringView s);
    
    bool icu4x_ExemplarCharacters_contains_mv1(const icu4x::capi::ExemplarCharacters* self, char32_t cp);
    
    typedef struct icu4x_ExemplarCharacters_try_new_main_mv1_result {union {icu4x::capi::ExemplarCharacters* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ExemplarCharacters_try_new_main_mv1_result;
    icu4x_ExemplarCharacters_try_new_main_mv1_result icu4x_ExemplarCharacters_try_new_main_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_ExemplarCharacters_try_new_auxiliary_mv1_result {union {icu4x::capi::ExemplarCharacters* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ExemplarCharacters_try_new_auxiliary_mv1_result;
    icu4x_ExemplarCharacters_try_new_auxiliary_mv1_result icu4x_ExemplarCharacters_try_new_auxiliary_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_ExemplarCharacters_try_new_punctuation_mv1_result {union {icu4x::capi::ExemplarCharacters* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ExemplarCharacters_try_new_punctuation_mv1_result;
    icu4x_ExemplarCharacters_try_new_punctuation_mv1_result icu4x_ExemplarCharacters_try_new_punctuation_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_ExemplarCharacters_try_new_numbers_mv1_result {union {icu4x::capi::ExemplarCharacters* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ExemplarCharacters_try_new_numbers_mv1_result;
    icu4x_ExemplarCharacters_try_new_numbers_mv1_result icu4x_ExemplarCharacters_try_new_numbers_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    typedef struct icu4x_ExemplarCharacters_try_new_index_mv1_result {union {icu4x::capi::ExemplarCharacters* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ExemplarCharacters_try_new_index_mv1_result;
    icu4x_ExemplarCharacters_try_new_index_mv1_result icu4x_ExemplarCharacters_try_new_index_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);
    
    
    void icu4x_ExemplarCharacters_destroy_mv1(ExemplarCharacters* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::ExemplarCharacters::contains(std::string_view s) const {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_contains_str_mv1(this->AsFFI(),
    {s.data(), s.size()});
  return result;
}

inline bool icu4x::ExemplarCharacters::contains(char32_t cp) const {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_contains_mv1(this->AsFFI(),
    cp);
  return result;
}

inline diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> icu4x::ExemplarCharacters::try_new_main(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_try_new_main_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::ExemplarCharacters>>(std::unique_ptr<icu4x::ExemplarCharacters>(icu4x::ExemplarCharacters::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> icu4x::ExemplarCharacters::try_new_auxiliary(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_try_new_auxiliary_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::ExemplarCharacters>>(std::unique_ptr<icu4x::ExemplarCharacters>(icu4x::ExemplarCharacters::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> icu4x::ExemplarCharacters::try_new_punctuation(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_try_new_punctuation_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::ExemplarCharacters>>(std::unique_ptr<icu4x::ExemplarCharacters>(icu4x::ExemplarCharacters::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> icu4x::ExemplarCharacters::try_new_numbers(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_try_new_numbers_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::ExemplarCharacters>>(std::unique_ptr<icu4x::ExemplarCharacters>(icu4x::ExemplarCharacters::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> icu4x::ExemplarCharacters::try_new_index(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
  auto result = icu4x::capi::icu4x_ExemplarCharacters_try_new_index_mv1(provider.AsFFI(),
    locale.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::ExemplarCharacters>>(std::unique_ptr<icu4x::ExemplarCharacters>(icu4x::ExemplarCharacters::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::ExemplarCharacters* icu4x::ExemplarCharacters::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::ExemplarCharacters*>(this);
}

inline icu4x::capi::ExemplarCharacters* icu4x::ExemplarCharacters::AsFFI() {
  return reinterpret_cast<icu4x::capi::ExemplarCharacters*>(this);
}

inline const icu4x::ExemplarCharacters* icu4x::ExemplarCharacters::FromFFI(const icu4x::capi::ExemplarCharacters* ptr) {
  return reinterpret_cast<const icu4x::ExemplarCharacters*>(ptr);
}

inline icu4x::ExemplarCharacters* icu4x::ExemplarCharacters::FromFFI(icu4x::capi::ExemplarCharacters* ptr) {
  return reinterpret_cast<icu4x::ExemplarCharacters*>(ptr);
}

inline void icu4x::ExemplarCharacters::operator delete(void* ptr) {
  icu4x::capi::icu4x_ExemplarCharacters_destroy_mv1(reinterpret_cast<icu4x::capi::ExemplarCharacters*>(ptr));
}


#endif // icu4x_ExemplarCharacters_HPP
