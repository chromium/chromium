// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/l10n/l10n_util_ios.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace l10n_util {
namespace {

// Helper version of GetDisplayNameForLocale() operating on NSString*.
// Note: this function may be called from any thread and it *must* be
// thread-safe. Thus attention must be paid if any caching is introduced.
NSString* GetDisplayNameForLocale(NSString* language,
                                  NSString* display_locale) {
  base::scoped_nsobject<NSLocale> ns_locale(
      [[NSLocale alloc] initWithLocaleIdentifier:display_locale]);

  NSString* localized_language_name =
      [ns_locale.get() displayNameForKey:NSLocaleIdentifier value:language];

  // Return localized language if system API provided it. Do not attempt to
  // manually parse into error format if no |locale| was provided.
  if (localized_language_name.length || !language.length) {
    return localized_language_name;
  }

  NSRange script_seperator = [language rangeOfString:@"-"
                                             options:NSBackwardsSearch];
  if (!script_seperator.length) {
    return [language lowercaseString];
  }

  NSString* language_component =
      [language substringToIndex:script_seperator.location];
  NSString* script_component = [language
      substringFromIndex:script_seperator.location + script_seperator.length];
  if (!script_component.length) {
    return [language_component lowercaseString];
  }

  return [NSString stringWithFormat:@"%@ (%@)",
                                    [language_component lowercaseString],
                                    [script_component uppercaseString]];
}

}  // namespace

std::u16string GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale) {
  return base::SysNSStringToUTF16(
      GetDisplayNameForLocale(base::SysUTF8ToNSString(locale),
                              base::SysUTF8ToNSString(display_locale)));
}

}  // namespace l10n_util
