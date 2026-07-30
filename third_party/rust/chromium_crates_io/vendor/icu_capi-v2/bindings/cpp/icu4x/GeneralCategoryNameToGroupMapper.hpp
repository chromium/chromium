#ifndef ICU4X_GeneralCategoryNameToGroupMapper_HPP
#define ICU4X_GeneralCategoryNameToGroupMapper_HPP

#include "GeneralCategoryNameToGroupMapper.d.hpp"

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
#include "GeneralCategoryGroup.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryNameToGroupMapper_get_strict_mv1(const icu4x::capi::GeneralCategoryNameToGroupMapper* self, icu4x::diplomat::capi::DiplomatStringView name);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryNameToGroupMapper_get_loose_mv1(const icu4x::capi::GeneralCategoryNameToGroupMapper* self, icu4x::diplomat::capi::DiplomatStringView name);

    icu4x::capi::GeneralCategoryNameToGroupMapper* icu4x_GeneralCategoryNameToGroupMapper_create_mv1(void);

    typedef struct icu4x_GeneralCategoryNameToGroupMapper_create_with_provider_mv1_result {union {icu4x::capi::GeneralCategoryNameToGroupMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_GeneralCategoryNameToGroupMapper_create_with_provider_mv1_result;
    icu4x_GeneralCategoryNameToGroupMapper_create_with_provider_mv1_result icu4x_GeneralCategoryNameToGroupMapper_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_GeneralCategoryNameToGroupMapper_destroy_mv1(GeneralCategoryNameToGroupMapper* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryNameToGroupMapper::get_strict(std::string_view name) const {
    auto result = icu4x::capi::icu4x_GeneralCategoryNameToGroupMapper_get_strict_mv1(this->AsFFI(),
        {name.data(), name.size()});
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryNameToGroupMapper::get_loose(std::string_view name) const {
    auto result = icu4x::capi::icu4x_GeneralCategoryNameToGroupMapper_get_loose_mv1(this->AsFFI(),
        {name.data(), name.size()});
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper> icu4x::GeneralCategoryNameToGroupMapper::create() {
    auto result = icu4x::capi::icu4x_GeneralCategoryNameToGroupMapper_create_mv1();
    return std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>(icu4x::GeneralCategoryNameToGroupMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>, icu4x::DataError> icu4x::GeneralCategoryNameToGroupMapper::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_GeneralCategoryNameToGroupMapper_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>>(std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>(icu4x::GeneralCategoryNameToGroupMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToGroupMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::GeneralCategoryNameToGroupMapper* icu4x::GeneralCategoryNameToGroupMapper::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::GeneralCategoryNameToGroupMapper*>(this);
}

inline icu4x::capi::GeneralCategoryNameToGroupMapper* icu4x::GeneralCategoryNameToGroupMapper::AsFFI() {
    return reinterpret_cast<icu4x::capi::GeneralCategoryNameToGroupMapper*>(this);
}

inline const icu4x::GeneralCategoryNameToGroupMapper* icu4x::GeneralCategoryNameToGroupMapper::FromFFI(const icu4x::capi::GeneralCategoryNameToGroupMapper* ptr) {
    return reinterpret_cast<const icu4x::GeneralCategoryNameToGroupMapper*>(ptr);
}

inline icu4x::GeneralCategoryNameToGroupMapper* icu4x::GeneralCategoryNameToGroupMapper::FromFFI(icu4x::capi::GeneralCategoryNameToGroupMapper* ptr) {
    return reinterpret_cast<icu4x::GeneralCategoryNameToGroupMapper*>(ptr);
}

inline void icu4x::GeneralCategoryNameToGroupMapper::operator delete(void* ptr) {
    icu4x::capi::icu4x_GeneralCategoryNameToGroupMapper_destroy_mv1(reinterpret_cast<icu4x::capi::GeneralCategoryNameToGroupMapper*>(ptr));
}


#endif // ICU4X_GeneralCategoryNameToGroupMapper_HPP
