#ifndef ICU4X_TimeZone_HPP
#define ICU4X_TimeZone_HPP

#include "TimeZone.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "TimeZoneInfo.hpp"
#include "UtcOffset.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::TimeZone* icu4x_TimeZone_unknown_mv1(void);

    bool icu4x_TimeZone_is_unknown_mv1(const icu4x::capi::TimeZone* self);

    icu4x::capi::TimeZone* icu4x_TimeZone_create_from_iana_id_mv1(icu4x::diplomat::capi::DiplomatStringView iana_id);

    icu4x::capi::TimeZone* icu4x_TimeZone_create_from_windows_id_mv1(icu4x::diplomat::capi::DiplomatStringView windows_id, icu4x::diplomat::capi::DiplomatStringView region);

    icu4x::capi::TimeZone* icu4x_TimeZone_create_from_system_id_mv1(icu4x::diplomat::capi::DiplomatStringView id, icu4x::diplomat::capi::DiplomatStringView _region);

    icu4x::capi::TimeZone* icu4x_TimeZone_create_from_bcp47_mv1(icu4x::diplomat::capi::DiplomatStringView id);

    icu4x::capi::TimeZoneInfo* icu4x_TimeZone_with_offset_mv1(const icu4x::capi::TimeZone* self, const icu4x::capi::UtcOffset* offset);

    icu4x::capi::TimeZoneInfo* icu4x_TimeZone_without_offset_mv1(const icu4x::capi::TimeZone* self);

    void icu4x_TimeZone_destroy_mv1(TimeZone* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::unknown() {
    auto result = icu4x::capi::icu4x_TimeZone_unknown_mv1();
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline bool icu4x::TimeZone::is_unknown() const {
    auto result = icu4x::capi::icu4x_TimeZone_is_unknown_mv1(this->AsFFI());
    return result;
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::create_from_iana_id(std::string_view iana_id) {
    auto result = icu4x::capi::icu4x_TimeZone_create_from_iana_id_mv1({iana_id.data(), iana_id.size()});
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::create_from_windows_id(std::string_view windows_id, std::string_view region) {
    auto result = icu4x::capi::icu4x_TimeZone_create_from_windows_id_mv1({windows_id.data(), windows_id.size()},
        {region.data(), region.size()});
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::create_from_system_id(std::string_view id, std::string_view _region) {
    auto result = icu4x::capi::icu4x_TimeZone_create_from_system_id_mv1({id.data(), id.size()},
        {_region.data(), _region.size()});
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::create_from_bcp47(std::string_view id) {
    auto result = icu4x::capi::icu4x_TimeZone_create_from_bcp47_mv1({id.data(), id.size()});
    return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZone::with_offset(const icu4x::UtcOffset& offset) const {
    auto result = icu4x::capi::icu4x_TimeZone_with_offset_mv1(this->AsFFI(),
        offset.AsFFI());
    return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZone::without_offset() const {
    auto result = icu4x::capi::icu4x_TimeZone_without_offset_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline const icu4x::capi::TimeZone* icu4x::TimeZone::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::TimeZone*>(this);
}

inline icu4x::capi::TimeZone* icu4x::TimeZone::AsFFI() {
    return reinterpret_cast<icu4x::capi::TimeZone*>(this);
}

inline const icu4x::TimeZone* icu4x::TimeZone::FromFFI(const icu4x::capi::TimeZone* ptr) {
    return reinterpret_cast<const icu4x::TimeZone*>(ptr);
}

inline icu4x::TimeZone* icu4x::TimeZone::FromFFI(icu4x::capi::TimeZone* ptr) {
    return reinterpret_cast<icu4x::TimeZone*>(ptr);
}

inline void icu4x::TimeZone::operator delete(void* ptr) {
    icu4x::capi::icu4x_TimeZone_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZone*>(ptr));
}


#endif // ICU4X_TimeZone_HPP
