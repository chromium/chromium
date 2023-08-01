// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util_mac_bridge.h"

@implementation L10nUtils

+ (NSString*)stringForMessageID:(int)messageID {
  return l10n_util::GetNSString(messageID);
}

+ (NSString*)stringWithFixupForMessageID:(int)messageID {
  return l10n_util::GetNSStringWithFixup(messageID);
}

+ (NSString*)formatStringForMessageID:(int)messageID
                             argument:(NSString*)argument {
  return l10n_util::GetNSStringF(messageID, base::SysNSStringToUTF16(argument));
}

+ (NSString*)pluralStringForMessageID:(int)messageID number:(NSInteger)number {
  int numberInt = static_cast<int>(number);
  return l10n_util::GetPluralNSStringF(messageID, numberInt);
}

@end
