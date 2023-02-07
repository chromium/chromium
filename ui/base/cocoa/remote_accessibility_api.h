// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_REMOTE_ACCESSIBILITY_API_H_
#define UI_BASE_COCOA_REMOTE_ACCESSIBILITY_API_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"

@interface NSAccessibilityRemoteUIElement : NSObject
+ (void)registerRemoteUIProcessIdentifier:(int)pid;
+ (NSData*)remoteTokenForLocalUIElement:(id)element;
- (id)initWithRemoteToken:(NSData*)token;
@property(retain) id windowUIElement;
@property(retain) id topLevelUIElement;
@end

namespace ui {

// Helper functions to implement the above functions using std::vectors intsead
// of NSData.
class COMPONENT_EXPORT(UI_BASE) RemoteAccessibility {
 public:
  static std::vector<uint8_t> GetTokenForLocalElement(id element);
  static base::scoped_nsobject<NSAccessibilityRemoteUIElement>
  GetRemoteElementFromToken(const std::vector<uint8_t>& token);
};

}  // namespace ui

#endif  // UI_BASE_COCOA_REMOTE_ACCESSIBILITY_API_H_
