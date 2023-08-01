// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor.h"

#import <UIKit/UIKit.h>

namespace ui {

DeviceFormFactor GetDeviceFormFactor() {
  UIUserInterfaceIdiom idiom = UIDevice.currentDevice.userInterfaceIdiom;
  if (idiom == UIUserInterfaceIdiomPad)
    return DEVICE_FORM_FACTOR_TABLET;
  return DEVICE_FORM_FACTOR_PHONE;
}

}  // namespace ui
