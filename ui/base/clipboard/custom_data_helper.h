// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_
#define UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
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
void ReadCustomDataTypes(const void* data,
                         size_t data_length,
                         std::vector<std::u16string>* types);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void ReadCustomDataForType(const void* data,
                           size_t data_length,
                           const std::u16string& type,
                           std::u16string* result);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void ReadCustomDataIntoMap(
    const void* data,
    size_t data_length,
    std::unordered_map<std::u16string, std::u16string>* result);

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
