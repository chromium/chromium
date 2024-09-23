// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/remote_accessibility_api.h"

#import "base/apple/foundation_util.h"

namespace ui {

// static
std::vector<uint8_t> RemoteAccessibility::GetTokenForLocalElement(id element) {
  NSData* data =
      [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:element];
  auto span = base::apple::NSDataToSpan(data);
  return {span.begin(), span.end()};
}

// static
NSAccessibilityRemoteUIElement* RemoteAccessibility::GetRemoteElementFromToken(
    const std::vector<uint8_t>& token) {
  if (token.empty()) {
    return nil;
  }
  NSData* data = [[NSData alloc] initWithBytes:token.data()
                                        length:token.size()];
  return [[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:data];
}

}  // namespace ui
