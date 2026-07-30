#ifndef ICU4X_EmojiSetData_D_HPP
#define ICU4X_EmojiSetData_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct EmojiSetData; }
class EmojiSetData;
class DataError;
} // namespace icu4x



namespace icu4x {
namespace capi {
    struct EmojiSetData;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.
 *
 * See the [Rust documentation for `properties`](https://docs.rs/icu/2.2.0/icu/properties/index.html) for more information.
 *
 * See the [Rust documentation for `EmojiSetData`](https://docs.rs/icu/2.2.0/icu/properties/struct.EmojiSetData.html) for more information.
 *
 * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/properties/struct.EmojiSetData.html#method.new) for more information.
 *
 * See the [Rust documentation for `EmojiSetDataBorrowed`](https://docs.rs/icu/2.2.0/icu/properties/struct.EmojiSetDataBorrowed.html) for more information.
 */
class EmojiSetData {
public:

  /**
   * Checks whether the string is in the set.
   *
   * See the [Rust documentation for `contains_str`](https://docs.rs/icu/2.2.0/icu/properties/struct.EmojiSetDataBorrowed.html#method.contains_str) for more information.
   */
  inline bool contains(std::string_view s) const;

  /**
   * Checks whether the code point is in the set.
   *
   * See the [Rust documentation for `contains`](https://docs.rs/icu/2.2.0/icu/properties/struct.EmojiSetDataBorrowed.html#method.contains) for more information.
   */
  inline bool contains(char32_t cp) const;

  /**
   * Create a map for the `Basic_Emoji` property, using compiled data.
   *
   * See the [Rust documentation for `BasicEmoji`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BasicEmoji.html) for more information.
   */
  inline static std::unique_ptr<icu4x::EmojiSetData> create_basic();

  /**
   * Create a map for the `Basic_Emoji` property, using a particular data source.
   *
   * See the [Rust documentation for `BasicEmoji`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.BasicEmoji.html) for more information.
   */
  inline static icu4x::diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError> create_basic_with_provider(const icu4x::DataProvider& provider);

  /**
   * Get the `Basic_Emoji` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EmojiSet.html#tymethod.for_char) for more information.
   */
  inline static bool basic_emoji_for_char(char32_t ch);

  /**
   * Get the `Basic_Emoji` value for a given character, using compiled data
   *
   * See the [Rust documentation for `for_str`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EmojiSet.html#tymethod.for_str) for more information.
   */
  inline static bool basic_emoji_for_str(std::string_view s);

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
#endif // ICU4X_EmojiSetData_D_HPP
