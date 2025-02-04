#ifndef icu4x_EmojiSetData_HPP
#define icu4x_EmojiSetData_HPP

#include "EmojiSetData.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    bool icu4x_EmojiSetData_contains_str_mv1(const icu4x::capi::EmojiSetData* self, diplomat::capi::DiplomatStringView s);
    
    bool icu4x_EmojiSetData_contains_mv1(const icu4x::capi::EmojiSetData* self, char32_t cp);
    
    typedef struct icu4x_EmojiSetData_load_basic_mv1_result {union {icu4x::capi::EmojiSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_EmojiSetData_load_basic_mv1_result;
    icu4x_EmojiSetData_load_basic_mv1_result icu4x_EmojiSetData_load_basic_mv1(const icu4x::capi::DataProvider* provider);
    
    
    void icu4x_EmojiSetData_destroy_mv1(EmojiSetData* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::EmojiSetData::contains(std::string_view s) const {
  auto result = icu4x::capi::icu4x_EmojiSetData_contains_str_mv1(this->AsFFI(),
    {s.data(), s.size()});
  return result;
}

inline bool icu4x::EmojiSetData::contains(char32_t cp) const {
  auto result = icu4x::capi::icu4x_EmojiSetData_contains_mv1(this->AsFFI(),
    cp);
  return result;
}

inline diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError> icu4x::EmojiSetData::load_basic(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_EmojiSetData_load_basic_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::EmojiSetData>>(std::unique_ptr<icu4x::EmojiSetData>(icu4x::EmojiSetData::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::EmojiSetData* icu4x::EmojiSetData::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::EmojiSetData*>(this);
}

inline icu4x::capi::EmojiSetData* icu4x::EmojiSetData::AsFFI() {
  return reinterpret_cast<icu4x::capi::EmojiSetData*>(this);
}

inline const icu4x::EmojiSetData* icu4x::EmojiSetData::FromFFI(const icu4x::capi::EmojiSetData* ptr) {
  return reinterpret_cast<const icu4x::EmojiSetData*>(ptr);
}

inline icu4x::EmojiSetData* icu4x::EmojiSetData::FromFFI(icu4x::capi::EmojiSetData* ptr) {
  return reinterpret_cast<icu4x::EmojiSetData*>(ptr);
}

inline void icu4x::EmojiSetData::operator delete(void* ptr) {
  icu4x::capi::icu4x_EmojiSetData_destroy_mv1(reinterpret_cast<icu4x::capi::EmojiSetData*>(ptr));
}


#endif // icu4x_EmojiSetData_HPP
