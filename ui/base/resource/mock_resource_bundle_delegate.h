// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_
#define UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"

namespace ui {

class MockResourceBundleDelegate : public ResourceBundle::Delegate {
 public:
  MockResourceBundleDelegate();
  ~MockResourceBundleDelegate() override;

  MOCK_METHOD2(GetPathForResourcePack,
               base::FilePath(const base::FilePath& pack_path,
                              ScaleFactor scale_factor));
  MOCK_METHOD2(GetPathForLocalePack,
               base::FilePath(const base::FilePath& pack_path,
                              const std::string& locale));
  MOCK_METHOD1(GetImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD1(GetNativeImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD2(LoadDataResourceBytes,
               base::RefCountedMemory*(int resource_id,
                                       ScaleFactor scale_factor));
  MOCK_CONST_METHOD3(GetRawDataResource,
                     bool(int resource_id,
                          ScaleFactor scale_factor,
                          base::StringPiece* value));
  MOCK_CONST_METHOD2(GetLocalizedString,
                     bool(int message_id, base::string16* value));
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_MOCK_RESOURCE_BUNDLE_DELEGATE_H_
