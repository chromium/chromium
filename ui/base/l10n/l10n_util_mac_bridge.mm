// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util_mac_bridge.h"

@implementation L10NUtils

+ (NSString*)stringForMessageId:(int)messageId {
  return l10n_util::GetNSString(messageId);
}

+ (NSString*)stringWithFixupForMessageId:(int)messageId {
  return l10n_util::GetNSStringWithFixup(messageId);
}

@end
