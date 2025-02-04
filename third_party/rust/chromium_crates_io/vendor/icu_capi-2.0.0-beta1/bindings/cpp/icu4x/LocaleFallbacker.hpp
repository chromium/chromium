#ifndef icu4x_LocaleFallbacker_HPP
#define icu4x_LocaleFallbacker_HPP

#include "LocaleFallbacker.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "LocaleFallbackConfig.hpp"
#include "LocaleFallbackerWithConfig.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_LocaleFallbacker_create_mv1_result {union {icu4x::capi::LocaleFallbacker* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LocaleFallbacker_create_mv1_result;
    icu4x_LocaleFallbacker_create_mv1_result icu4x_LocaleFallbacker_create_mv1(const icu4x::capi::DataProvider* provider);
    
    icu4x::capi::LocaleFallbacker* icu4x_LocaleFallbacker_without_data_mv1(void);
    
    icu4x::capi::LocaleFallbackerWithConfig* icu4x_LocaleFallbacker_for_config_mv1(const icu4x::capi::LocaleFallbacker* self, icu4x::capi::LocaleFallbackConfig config);
    
    
    void icu4x_LocaleFallbacker_destroy_mv1(LocaleFallbacker* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::LocaleFallbacker>, icu4x::DataError> icu4x::LocaleFallbacker::create(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_LocaleFallbacker_create_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LocaleFallbacker>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LocaleFallbacker>>(std::unique_ptr<icu4x::LocaleFallbacker>(icu4x::LocaleFallbacker::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LocaleFallbacker>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::LocaleFallbacker> icu4x::LocaleFallbacker::without_data() {
  auto result = icu4x::capi::icu4x_LocaleFallbacker_without_data_mv1();
  return std::unique_ptr<icu4x::LocaleFallbacker>(icu4x::LocaleFallbacker::FromFFI(result));
}

inline std::unique_ptr<icu4x::LocaleFallbackerWithConfig> icu4x::LocaleFallbacker::for_config(icu4x::LocaleFallbackConfig config) const {
  auto result = icu4x::capi::icu4x_LocaleFallbacker_for_config_mv1(this->AsFFI(),
    config.AsFFI());
  return std::unique_ptr<icu4x::LocaleFallbackerWithConfig>(icu4x::LocaleFallbackerWithConfig::FromFFI(result));
}

inline const icu4x::capi::LocaleFallbacker* icu4x::LocaleFallbacker::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::LocaleFallbacker*>(this);
}

inline icu4x::capi::LocaleFallbacker* icu4x::LocaleFallbacker::AsFFI() {
  return reinterpret_cast<icu4x::capi::LocaleFallbacker*>(this);
}

inline const icu4x::LocaleFallbacker* icu4x::LocaleFallbacker::FromFFI(const icu4x::capi::LocaleFallbacker* ptr) {
  return reinterpret_cast<const icu4x::LocaleFallbacker*>(ptr);
}

inline icu4x::LocaleFallbacker* icu4x::LocaleFallbacker::FromFFI(icu4x::capi::LocaleFallbacker* ptr) {
  return reinterpret_cast<icu4x::LocaleFallbacker*>(ptr);
}

inline void icu4x::LocaleFallbacker::operator delete(void* ptr) {
  icu4x::capi::icu4x_LocaleFallbacker_destroy_mv1(reinterpret_cast<icu4x::capi::LocaleFallbacker*>(ptr));
}


#endif // icu4x_LocaleFallbacker_HPP
