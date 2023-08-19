// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/l10n/l10n_util_ios.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_local.h"

namespace l10n_util {
namespace {

// Class used to implement a per thread cache of the NSLocale.
// TODO(http://www.openradar.me/39659413): It would be nice to simplify most of
// this class to a simple `thread_local NSLocale*`, but that causes Clang to
// emit "error: thread-local variable has non-trivial ownership: type is
// 'NSLocale *__strong'".
class LocaleCache {
 public:
  static NSLocale* LocaleForIdentifier(NSString* identifier) {
    using TLSLocaleCache = base::ThreadLocalOwnedPointer<LocaleCache>;
    static base::NoDestructor<TLSLocaleCache> gInstance;

    TLSLocaleCache& tls_cache = *gInstance;
    LocaleCache* cache = tls_cache.Get();
    if (!cache) {
      tls_cache.Set(std::make_unique<LocaleCache>());
      cache = tls_cache.Get();
    }

    DCHECK(cache);
    return cache->LocaleForIdentifierInternal(identifier);
  }

 private:
  NSLocale* LocaleForIdentifierInternal(NSString* identifier) {
    if (!locale_ || ![locale_.localeIdentifier isEqualToString:identifier]) {
      locale_ = [[NSLocale alloc] initWithLocaleIdentifier:identifier];
    }

    DCHECK(locale_);
    return locale_;
  }

  __strong NSLocale* locale_;
};

// Helper version of GetDisplayNameForLocale() operating on NSString*.
// Note: this function may be called from any thread and it *must* be
// thread-safe. Thus attention must be paid if any caching is introduced.
NSString* GetDisplayNameForLocale(NSString* language,
                                  NSString* display_locale) {
  NSLocale* ns_locale = LocaleCache::LocaleForIdentifier(display_locale);
  NSString* localized_language_name =
      [ns_locale displayNameForKey:NSLocaleIdentifier value:language];

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
