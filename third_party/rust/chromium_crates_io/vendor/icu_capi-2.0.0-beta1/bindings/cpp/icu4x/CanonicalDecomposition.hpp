#ifndef icu4x_CanonicalDecomposition_HPP
#define icu4x_CanonicalDecomposition_HPP

#include "CanonicalDecomposition.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "Decomposed.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_CanonicalDecomposition_create_mv1_result {union {icu4x::capi::CanonicalDecomposition* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CanonicalDecomposition_create_mv1_result;
    icu4x_CanonicalDecomposition_create_mv1_result icu4x_CanonicalDecomposition_create_mv1(const icu4x::capi::DataProvider* provider);
    
    icu4x::capi::Decomposed icu4x_CanonicalDecomposition_decompose_mv1(const icu4x::capi::CanonicalDecomposition* self, char32_t c);
    
    
    void icu4x_CanonicalDecomposition_destroy_mv1(CanonicalDecomposition* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::CanonicalDecomposition>, icu4x::DataError> icu4x::CanonicalDecomposition::create(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CanonicalDecomposition_create_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CanonicalDecomposition>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CanonicalDecomposition>>(std::unique_ptr<icu4x::CanonicalDecomposition>(icu4x::CanonicalDecomposition::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CanonicalDecomposition>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::Decomposed icu4x::CanonicalDecomposition::decompose(char32_t c) const {
  auto result = icu4x::capi::icu4x_CanonicalDecomposition_decompose_mv1(this->AsFFI(),
    c);
  return icu4x::Decomposed::FromFFI(result);
}

inline const icu4x::capi::CanonicalDecomposition* icu4x::CanonicalDecomposition::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::CanonicalDecomposition*>(this);
}

inline icu4x::capi::CanonicalDecomposition* icu4x::CanonicalDecomposition::AsFFI() {
  return reinterpret_cast<icu4x::capi::CanonicalDecomposition*>(this);
}

inline const icu4x::CanonicalDecomposition* icu4x::CanonicalDecomposition::FromFFI(const icu4x::capi::CanonicalDecomposition* ptr) {
  return reinterpret_cast<const icu4x::CanonicalDecomposition*>(ptr);
}

inline icu4x::CanonicalDecomposition* icu4x::CanonicalDecomposition::FromFFI(icu4x::capi::CanonicalDecomposition* ptr) {
  return reinterpret_cast<icu4x::CanonicalDecomposition*>(ptr);
}

inline void icu4x::CanonicalDecomposition::operator delete(void* ptr) {
  icu4x::capi::icu4x_CanonicalDecomposition_destroy_mv1(reinterpret_cast<icu4x::capi::CanonicalDecomposition*>(ptr));
}


#endif // icu4x_CanonicalDecomposition_HPP
