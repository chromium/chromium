// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/remote_accessibility_api.h"

namespace ui {

// static
std::vector<uint8_t> RemoteAccessibility::GetTokenForLocalElement(id element) {
  NSData* data =
      [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:element];
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>([data bytes]);
  return std::vector<uint8_t>(bytes, bytes + [data length]);
}

// static
base::scoped_nsobject<NSAccessibilityRemoteUIElement>
RemoteAccessibility::GetRemoteElementFromToken(
    const std::vector<uint8_t>& token) {
  if (token.empty())
    return base::scoped_nsobject<NSAccessibilityRemoteUIElement>();
  base::scoped_nsobject<NSData> data(
      [[NSData alloc] initWithBytes:token.data() length:token.size()]);
  return base::scoped_nsobject<NSAccessibilityRemoteUIElement>(
      [[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:data]);
}

}  // namespace ui
