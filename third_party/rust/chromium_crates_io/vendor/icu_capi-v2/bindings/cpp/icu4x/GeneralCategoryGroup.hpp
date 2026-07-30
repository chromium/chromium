#ifndef ICU4X_GeneralCategoryGroup_HPP
#define ICU4X_GeneralCategoryGroup_HPP

#include "GeneralCategoryGroup.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "GeneralCategory.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    bool icu4x_GeneralCategoryGroup_contains_mv1(icu4x::capi::GeneralCategoryGroup self, icu4x::capi::GeneralCategory val);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_complement_mv1(icu4x::capi::GeneralCategoryGroup self);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_all_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_empty_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_union_mv1(icu4x::capi::GeneralCategoryGroup self, icu4x::capi::GeneralCategoryGroup other);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_intersection_mv1(icu4x::capi::GeneralCategoryGroup self, icu4x::capi::GeneralCategoryGroup other);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_cased_letter_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_letter_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_mark_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_number_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_separator_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_other_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_punctuation_mv1(void);

    icu4x::capi::GeneralCategoryGroup icu4x_GeneralCategoryGroup_symbol_mv1(void);

    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::GeneralCategoryGroup::contains(icu4x::GeneralCategory val) const {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_contains_mv1(this->AsFFI(),
        val.AsFFI());
    return result;
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::complement() const {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_complement_mv1(this->AsFFI());
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::all() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_all_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::empty() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_empty_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::union_(icu4x::GeneralCategoryGroup other) const {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_union_mv1(this->AsFFI(),
        other.AsFFI());
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::intersection(icu4x::GeneralCategoryGroup other) const {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_intersection_mv1(this->AsFFI(),
        other.AsFFI());
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::cased_letter() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_cased_letter_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::letter() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_letter_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::mark() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_mark_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::number() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_number_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::separator() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_separator_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::other() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_other_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::punctuation() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_punctuation_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::symbol() {
    auto result = icu4x::capi::icu4x_GeneralCategoryGroup_symbol_mv1();
    return icu4x::GeneralCategoryGroup::FromFFI(result);
}


inline icu4x::capi::GeneralCategoryGroup icu4x::GeneralCategoryGroup::AsFFI() const {
    return icu4x::capi::GeneralCategoryGroup {
        /* .mask = */ mask,
    };
}

inline icu4x::GeneralCategoryGroup icu4x::GeneralCategoryGroup::FromFFI(icu4x::capi::GeneralCategoryGroup c_struct) {
    return icu4x::GeneralCategoryGroup {
        /* .mask = */ c_struct.mask,
    };
}


#endif // ICU4X_GeneralCategoryGroup_HPP
