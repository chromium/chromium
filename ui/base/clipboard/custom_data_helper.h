// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_
#define UI_BASE_CLIPBOARD_CUSTOM_DATA_HELPER_H_

#include <stddef.h>

#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

// Due to restrictions of most operating systems, we don't directly map each
// type of custom data to a native data transfer type. Instead, we serialize
// each key-value pair into the pickle as a pair of string objects, and then
// write the binary data in the pickle to the native data transfer object.
namespace base {
class Pickle;
}

namespace ui {

COMPONENT_EXPORT(BASE_CLIPBOARD)
void ReadCustomDataTypes(const void* data,
                         size_t data_length,
                         std::vector<base::string16>* types);
COMPONENT_EXPORT(BASE_CLIPBOARD)
void ReadCustomDataForType(const void* data,
                           size_t data_length,
                           const base::string16& type,
                           base::string16* result);
COMPONENT_EXPORT(BASE_CLIPBOARD)
void ReadCustomDataIntoMap(
    const void* data,
    size_t data_length,
    std::unordered_map<base::string16, base::string16>* result);

COMPONENT_EXPORT(BASE_CLIPBOARD)
void WriteCustomDataToPickle(
    const std::unordered_map<base::string16, base::string16>& data,
    base::Pickle* pickle);

COMPONENT_EXPORT(BASE_CLIPBOARD)
void WriteCustomDataToPickle(
    const base::flat_map<base::string16, base::string16>& data,
    base::Pickle* pickle);

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_H_
