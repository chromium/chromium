// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_HANDLE_H_
#define UI_BASE_RESOURCE_RESOURCE_HANDLE_H_

#include <stdint.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/dcheck_is_on.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedStaticMemory;
}

namespace ui {

class COMPONENT_EXPORT(UI_DATA_PACK) ResourceHandle {
 public:
  // What type of encoding the text resources use.
  enum TextEncodingType {
    BINARY,
    UTF8,
    UTF16
  };

  virtual ~ResourceHandle() {}

  // Returns true if the DataPack contains a resource with id |resource_id|.
  virtual bool HasResource(uint16_t resource_id) const = 0;

  // Get resource by id |resource_id|, filling in |data|.
  // The data is owned by the DataPack object and should not be modified.
  // Returns false if the resource id isn't found.
  virtual std::optional<std::string_view> GetStringView(
      uint16_t resource_id) const = 0;

  // Like GetStringView(), but returns a reference to memory.
  // Caller owns the returned object.
  virtual base::RefCountedStaticMemory* GetStaticMemory(
      uint16_t resource_id) const = 0;

  // Get the encoding type of text resources.
  virtual TextEncodingType GetTextEncodingType() const = 0;

  // The scale of images in this resource pack relative to images in the 1x
  // resource pak.
  virtual ResourceScaleFactor GetResourceScaleFactor() const = 0;

#if DCHECK_IS_ON()
  // Checks to see if any resource in this DataPack already exists in the list
  // of resources.
  virtual void CheckForDuplicateResources(
      const std::vector<std::unique_ptr<ResourceHandle>>& packs) = 0;
#endif
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_HANDLE_H_
