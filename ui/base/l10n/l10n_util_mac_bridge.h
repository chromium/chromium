// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_
#define UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_

#import <UIKit/UIKit.h>

// An ObjC wrapper around namespaced C++ l10n methods.
@interface L10NUtils : NSObject

+ (NSString*)stringForMessageId:(int)messageId;

+ (NSString*)stringWithFixupForMessageId:(int)messageId;

+ (NSString*)formatStringForMessageId:(int)messageId
                             argument:(NSString*)argument;

@end

#endif  // UI_BASE_L10N_L10N_UTIL_MAC_BRIDGE_H_
