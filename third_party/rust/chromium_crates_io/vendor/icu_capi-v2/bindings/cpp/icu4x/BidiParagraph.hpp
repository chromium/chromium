#ifndef ICU4X_BidiParagraph_HPP
#define ICU4X_BidiParagraph_HPP

#include "BidiParagraph.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "BidiDirection.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    bool icu4x_BidiParagraph_set_paragraph_in_text_mv1(icu4x::capi::BidiParagraph* self, size_t n);

    icu4x::capi::BidiDirection icu4x_BidiParagraph_direction_mv1(const icu4x::capi::BidiParagraph* self);

    size_t icu4x_BidiParagraph_size_mv1(const icu4x::capi::BidiParagraph* self);

    size_t icu4x_BidiParagraph_range_start_mv1(const icu4x::capi::BidiParagraph* self);

    size_t icu4x_BidiParagraph_range_end_mv1(const icu4x::capi::BidiParagraph* self);

    typedef struct icu4x_BidiParagraph_reorder_line_mv1_result { bool is_ok;} icu4x_BidiParagraph_reorder_line_mv1_result;
    icu4x_BidiParagraph_reorder_line_mv1_result icu4x_BidiParagraph_reorder_line_mv1(const icu4x::capi::BidiParagraph* self, size_t range_start, size_t range_end, icu4x::diplomat::capi::DiplomatWrite* write);

    uint8_t icu4x_BidiParagraph_level_at_mv1(const icu4x::capi::BidiParagraph* self, size_t pos);

    void icu4x_BidiParagraph_destroy_mv1(BidiParagraph* self);

    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::BidiParagraph::set_paragraph_in_text(size_t n) {
    auto result = icu4x::capi::icu4x_BidiParagraph_set_paragraph_in_text_mv1(this->AsFFI(),
        n);
    return result;
}

inline icu4x::BidiDirection icu4x::BidiParagraph::direction() const {
    auto result = icu4x::capi::icu4x_BidiParagraph_direction_mv1(this->AsFFI());
    return icu4x::BidiDirection::FromFFI(result);
}

inline size_t icu4x::BidiParagraph::size() const {
    auto result = icu4x::capi::icu4x_BidiParagraph_size_mv1(this->AsFFI());
    return result;
}

inline size_t icu4x::BidiParagraph::range_start() const {
    auto result = icu4x::capi::icu4x_BidiParagraph_range_start_mv1(this->AsFFI());
    return result;
}

inline size_t icu4x::BidiParagraph::range_end() const {
    auto result = icu4x::capi::icu4x_BidiParagraph_range_end_mv1(this->AsFFI());
    return result;
}

inline std::optional<std::string> icu4x::BidiParagraph::reorder_line(size_t range_start, size_t range_end) const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    auto result = icu4x::capi::icu4x_BidiParagraph_reorder_line_mv1(this->AsFFI(),
        range_start,
        range_end,
        &write);
    return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}
template<typename W>
inline std::optional<std::monostate> icu4x::BidiParagraph::reorder_line_write(size_t range_start, size_t range_end, W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = icu4x::capi::icu4x_BidiParagraph_reorder_line_mv1(this->AsFFI(),
        range_start,
        range_end,
        &write);
    return result.is_ok ? std::optional<std::monostate>() : std::nullopt;
}

inline uint8_t icu4x::BidiParagraph::level_at(size_t pos) const {
    auto result = icu4x::capi::icu4x_BidiParagraph_level_at_mv1(this->AsFFI(),
        pos);
    return result;
}

inline const icu4x::capi::BidiParagraph* icu4x::BidiParagraph::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::BidiParagraph*>(this);
}

inline icu4x::capi::BidiParagraph* icu4x::BidiParagraph::AsFFI() {
    return reinterpret_cast<icu4x::capi::BidiParagraph*>(this);
}

inline const icu4x::BidiParagraph* icu4x::BidiParagraph::FromFFI(const icu4x::capi::BidiParagraph* ptr) {
    return reinterpret_cast<const icu4x::BidiParagraph*>(ptr);
}

inline icu4x::BidiParagraph* icu4x::BidiParagraph::FromFFI(icu4x::capi::BidiParagraph* ptr) {
    return reinterpret_cast<icu4x::BidiParagraph*>(ptr);
}

inline void icu4x::BidiParagraph::operator delete(void* ptr) {
    icu4x::capi::icu4x_BidiParagraph_destroy_mv1(reinterpret_cast<icu4x::capi::BidiParagraph*>(ptr));
}


#endif // ICU4X_BidiParagraph_HPP
