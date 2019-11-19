// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

#import <Foundation/Foundation.h>

namespace ui {

// TODO(dcheng): This name is temporary. See crbug.com/106449.
NSString* const kWebCustomDataPboardType = @"org.chromium.web-custom-data";
NSString* const kWebSmartPastePboardType = @"NeXT smart paste pasteboard type";
NSString* const kPepperCustomDataPboardType =
    @"org.chromium.pepper-custom-data";

}  // namespace ui
