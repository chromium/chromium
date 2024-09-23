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

  MOCK_METHOD2(GetPathForResourcePack,
               base::FilePath(const base::FilePath& pack_path,
                              ResourceScaleFactor scale_factor));
  MOCK_METHOD2(GetPathForLocalePack,
               base::FilePath(const base::FilePath& pack_path,
                              const std::string& locale));
  MOCK_METHOD1(GetImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD1(GetNativeImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD2(LoadDataResourceBytes,
               base::RefCountedMemory*(int resource_id,
                                       ResourceScaleFactor scale_factor));
  MOCK_METHOD1(LoadDataResourceString,
               std::optional<std::string>(int resource_id));
  MOCK_CONST_METHOD3(GetRawDataResource,
                     bool(int resource_id,
                          ResourceScaleFactor scale_factor,
                          std::string_view* value));
  MOCK_CONST_METHOD2(GetLocalizedString,
                     bool(int message_id, std::u16string* value));
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_
