// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_
#define UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "ui/base/resource/resource_handle.h"

namespace ui {

class ResourceDataDLL : public ResourceHandle {
 public:
  explicit ResourceDataDLL(HINSTANCE module);

  ResourceDataDLL(const ResourceDataDLL&) = delete;
  ResourceDataDLL& operator=(const ResourceDataDLL&) = delete;

  ~ResourceDataDLL() override;

  // ResourceHandle implementation:
  bool HasResource(uint16_t resource_id) const override;
  std::optional<std::string_view> GetStringView(
      uint16_t resource_id) const override;
  base::RefCountedStaticMemory* GetStaticMemory(
      uint16_t resource_id) const override;
  TextEncodingType GetTextEncodingType() const override;
  ResourceScaleFactor GetResourceScaleFactor() const override;
#if DCHECK_IS_ON()
  void CheckForDuplicateResources(
      const std::vector<std::unique_ptr<ResourceHandle>>& packs) override {}
#endif

 private:
  const HINSTANCE module_;
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_
