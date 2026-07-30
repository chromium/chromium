#ifndef ICU4X_CodePointSetBuilder_HPP
#define ICU4X_CodePointSetBuilder_HPP

#include "CodePointSetBuilder.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointSetData.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::CodePointSetBuilder* icu4x_CodePointSetBuilder_create_mv1(void);

    icu4x::capi::CodePointSetData* icu4x_CodePointSetBuilder_build_mv1(icu4x::capi::CodePointSetBuilder* self);

    void icu4x_CodePointSetBuilder_complement_mv1(icu4x::capi::CodePointSetBuilder* self);

    bool icu4x_CodePointSetBuilder_is_empty_mv1(const icu4x::capi::CodePointSetBuilder* self);

    void icu4x_CodePointSetBuilder_add_char_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t ch);

    void icu4x_CodePointSetBuilder_add_inclusive_range_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t start, char32_t end);

    void icu4x_CodePointSetBuilder_add_set_mv1(icu4x::capi::CodePointSetBuilder* self, const icu4x::capi::CodePointSetData* data);

    void icu4x_CodePointSetBuilder_remove_char_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t ch);

    void icu4x_CodePointSetBuilder_remove_inclusive_range_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t start, char32_t end);

    void icu4x_CodePointSetBuilder_remove_set_mv1(icu4x::capi::CodePointSetBuilder* self, const icu4x::capi::CodePointSetData* data);

    void icu4x_CodePointSetBuilder_retain_char_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t ch);

    void icu4x_CodePointSetBuilder_retain_inclusive_range_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t start, char32_t end);

    void icu4x_CodePointSetBuilder_retain_set_mv1(icu4x::capi::CodePointSetBuilder* self, const icu4x::capi::CodePointSetData* data);

    void icu4x_CodePointSetBuilder_complement_char_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t ch);

    void icu4x_CodePointSetBuilder_complement_inclusive_range_mv1(icu4x::capi::CodePointSetBuilder* self, char32_t start, char32_t end);

    void icu4x_CodePointSetBuilder_complement_set_mv1(icu4x::capi::CodePointSetBuilder* self, const icu4x::capi::CodePointSetData* data);

    void icu4x_CodePointSetBuilder_destroy_mv1(CodePointSetBuilder* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::CodePointSetBuilder> icu4x::CodePointSetBuilder::create() {
    auto result = icu4x::capi::icu4x_CodePointSetBuilder_create_mv1();
    return std::unique_ptr<icu4x::CodePointSetBuilder>(icu4x::CodePointSetBuilder::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointSetBuilder::build() {
    auto result = icu4x::capi::icu4x_CodePointSetBuilder_build_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline void icu4x::CodePointSetBuilder::complement() {
    icu4x::capi::icu4x_CodePointSetBuilder_complement_mv1(this->AsFFI());
}

inline bool icu4x::CodePointSetBuilder::is_empty() const {
    auto result = icu4x::capi::icu4x_CodePointSetBuilder_is_empty_mv1(this->AsFFI());
    return result;
}

inline void icu4x::CodePointSetBuilder::add_char(char32_t ch) {
    icu4x::capi::icu4x_CodePointSetBuilder_add_char_mv1(this->AsFFI(),
        ch);
}

inline void icu4x::CodePointSetBuilder::add_inclusive_range(char32_t start, char32_t end) {
    icu4x::capi::icu4x_CodePointSetBuilder_add_inclusive_range_mv1(this->AsFFI(),
        start,
        end);
}

inline void icu4x::CodePointSetBuilder::add_set(const icu4x::CodePointSetData& data) {
    icu4x::capi::icu4x_CodePointSetBuilder_add_set_mv1(this->AsFFI(),
        data.AsFFI());
}

inline void icu4x::CodePointSetBuilder::remove_char(char32_t ch) {
    icu4x::capi::icu4x_CodePointSetBuilder_remove_char_mv1(this->AsFFI(),
        ch);
}

inline void icu4x::CodePointSetBuilder::remove_inclusive_range(char32_t start, char32_t end) {
    icu4x::capi::icu4x_CodePointSetBuilder_remove_inclusive_range_mv1(this->AsFFI(),
        start,
        end);
}

inline void icu4x::CodePointSetBuilder::remove_set(const icu4x::CodePointSetData& data) {
    icu4x::capi::icu4x_CodePointSetBuilder_remove_set_mv1(this->AsFFI(),
        data.AsFFI());
}

inline void icu4x::CodePointSetBuilder::retain_char(char32_t ch) {
    icu4x::capi::icu4x_CodePointSetBuilder_retain_char_mv1(this->AsFFI(),
        ch);
}

inline void icu4x::CodePointSetBuilder::retain_inclusive_range(char32_t start, char32_t end) {
    icu4x::capi::icu4x_CodePointSetBuilder_retain_inclusive_range_mv1(this->AsFFI(),
        start,
        end);
}

inline void icu4x::CodePointSetBuilder::retain_set(const icu4x::CodePointSetData& data) {
    icu4x::capi::icu4x_CodePointSetBuilder_retain_set_mv1(this->AsFFI(),
        data.AsFFI());
}

inline void icu4x::CodePointSetBuilder::complement_char(char32_t ch) {
    icu4x::capi::icu4x_CodePointSetBuilder_complement_char_mv1(this->AsFFI(),
        ch);
}

inline void icu4x::CodePointSetBuilder::complement_inclusive_range(char32_t start, char32_t end) {
    icu4x::capi::icu4x_CodePointSetBuilder_complement_inclusive_range_mv1(this->AsFFI(),
        start,
        end);
}

inline void icu4x::CodePointSetBuilder::complement_set(const icu4x::CodePointSetData& data) {
    icu4x::capi::icu4x_CodePointSetBuilder_complement_set_mv1(this->AsFFI(),
        data.AsFFI());
}

inline const icu4x::capi::CodePointSetBuilder* icu4x::CodePointSetBuilder::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CodePointSetBuilder*>(this);
}

inline icu4x::capi::CodePointSetBuilder* icu4x::CodePointSetBuilder::AsFFI() {
    return reinterpret_cast<icu4x::capi::CodePointSetBuilder*>(this);
}

inline const icu4x::CodePointSetBuilder* icu4x::CodePointSetBuilder::FromFFI(const icu4x::capi::CodePointSetBuilder* ptr) {
    return reinterpret_cast<const icu4x::CodePointSetBuilder*>(ptr);
}

inline icu4x::CodePointSetBuilder* icu4x::CodePointSetBuilder::FromFFI(icu4x::capi::CodePointSetBuilder* ptr) {
    return reinterpret_cast<icu4x::CodePointSetBuilder*>(ptr);
}

inline void icu4x::CodePointSetBuilder::operator delete(void* ptr) {
    icu4x::capi::icu4x_CodePointSetBuilder_destroy_mv1(reinterpret_cast<icu4x::capi::CodePointSetBuilder*>(ptr));
}


#endif // ICU4X_CodePointSetBuilder_HPP
