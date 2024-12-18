// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_
#define UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_

#include <string>
#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace ui {

class MockResourceBundleDelegate : public ResourceBundle::Delegate {
 public:
  MockResourceBundleDelegate();
  ~MockResourceBundleDelegate() override;

  MOCK_METHOD(base::FilePath,
              GetPathForResourcePack,
              (const base::FilePath& pack_path,
               ResourceScaleFactor scale_factor),
              (override));
  MOCK_METHOD(base::FilePath,
              GetPathForLocalePack,
              (const base::FilePath& pack_path, const std::string& locale),
              (override));
  MOCK_METHOD(gfx::Image, GetImageNamed, (int resource_id), (override));
  MOCK_METHOD(gfx::Image, GetNativeImageNamed, (int resource_id), (override));
  MOCK_METHOD(bool, HasDataResource, (int resource_id), (const override));
  MOCK_METHOD(base::RefCountedMemory*,
              LoadDataResourceBytes,
              (int resource_id, ResourceScaleFactor scale_factor),
              (override));
  MOCK_METHOD(std::optional<std::string>,
              LoadDataResourceString,
              (int resource_id),
              (override));
  MOCK_METHOD(bool,
              GetRawDataResource,
              (int resource_id,
               ResourceScaleFactor scale_factor,
               std::string_view* value),
              (const override));
  MOCK_METHOD(bool,
              GetLocalizedString,
              (int message_id, std::u16string* value),
              (const override));
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_
