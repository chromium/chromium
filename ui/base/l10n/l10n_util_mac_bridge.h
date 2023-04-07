// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_
#define UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_

#import <UIKit/UIKit.h>

typedef int MessageID;

// An Objective-C wrapper around namespaced C++ l10n methods.
@interface L10nUtils : NSObject

+ (NSString*)stringForMessageID:(MessageID)messageID
    NS_SWIFT_NAME(string(messageId:));

+ (NSString*)stringWithFixupForMessageID:(MessageID)messageID
    NS_SWIFT_NAME(stringWithFixup(messageId:));

+ (NSString*)formatStringForMessageID:(MessageID)messageID
                             argument:(NSString*)argument
    NS_SWIFT_NAME(formatString(messageId:argument:));

+ (NSString*)pluralStringForMessageID:(MessageID)messageID
                               number:(NSInteger)number
    NS_SWIFT_NAME(pluralString(messageId:number:));

@end

#endif  // UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_
