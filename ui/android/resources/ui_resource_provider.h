// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_UI_RESOURCE_PROVIDER_H_
#define UI_ANDROID_RESOURCES_UI_RESOURCE_PROVIDER_H_

#include "cc/resources/ui_resource_client.h"
#include "ui/android/ui_android_export.h"

namespace ui {

class UI_ANDROID_EXPORT UIResourceProvider {
 public:
  virtual cc::UIResourceId CreateUIResource(cc::UIResourceClient* client) = 0;
  virtual void DeleteUIResource(cc::UIResourceId resource_id) = 0;
  virtual bool SupportsETC1NonPowerOfTwo() const = 0;

 protected:
  virtual ~UIResourceProvider() {}
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_UI_RESOURCE_PROVIDER_H_
