// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/remote_accessibility_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ui {

// static
std::vector<uint8_t> RemoteAccessibility::GetTokenForLocalElement(id element) {
  NSData* data =
      [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:element];
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.bytes);
  return std::vector<uint8_t>(bytes, bytes + data.length);
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
