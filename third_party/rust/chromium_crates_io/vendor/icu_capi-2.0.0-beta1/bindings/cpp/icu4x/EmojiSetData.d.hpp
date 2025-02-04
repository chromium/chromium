#ifndef icu4x_EmojiSetData_D_HPP
#define icu4x_EmojiSetData_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct EmojiSetData; }
class EmojiSetData;
class DataError;
}


namespace icu4x {
namespace capi {
    struct EmojiSetData;
} // namespace capi
} // namespace

namespace icu4x {
class EmojiSetData {
public:

  inline bool contains(std::string_view s) const;

  inline bool contains(char32_t cp) const;

  inline static diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError> load_basic(const icu4x::DataProvider& provider);

  inline const icu4x::capi::EmojiSetData* AsFFI() const;
  inline icu4x::capi::EmojiSetData* AsFFI();
  inline static const icu4x::EmojiSetData* FromFFI(const icu4x::capi::EmojiSetData* ptr);
  inline static icu4x::EmojiSetData* FromFFI(icu4x::capi::EmojiSetData* ptr);
  inline static void operator delete(void* ptr);
private:
  EmojiSetData() = delete;
  EmojiSetData(const icu4x::EmojiSetData&) = delete;
  EmojiSetData(icu4x::EmojiSetData&&) noexcept = delete;
  EmojiSetData operator=(const icu4x::EmojiSetData&) = delete;
  EmojiSetData operator=(icu4x::EmojiSetData&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_EmojiSetData_D_HPP
