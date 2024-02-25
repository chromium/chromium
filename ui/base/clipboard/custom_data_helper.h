// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_
#define UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "build/build_config.h"

// Due to restrictions of most operating systems, we don't directly map each
// type of custom data to a native data transfer type. Instead, we serialize
// each key-value pair into the pickle as a pair of string objects, and then
// write the binary data in the pickle to the native data transfer object.
namespace base {
class Pickle;
}

namespace ui {

COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void ReadCustomDataTypes(base::span<const uint8_t> data,
                         std::vector<std::u16string>* types);
[[nodiscard]] COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
    std::optional<std::u16string> ReadCustomDataForType(
        base::span<const uint8_t> data,
        std::u16string_view type);
[[nodiscard]] COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
    std::optional<std::unordered_map<
        std::u16string,
        std::u16string>> ReadCustomDataIntoMap(base::span<const uint8_t> data);

COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void WriteCustomDataToPickle(
    const std::unordered_map<std::u16string, std::u16string>& data,
    base::Pickle* pickle);

COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void WriteCustomDataToPickle(
    const base::flat_map<std::u16string, std::u16string>& data,
    base::Pickle* pickle);

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_
