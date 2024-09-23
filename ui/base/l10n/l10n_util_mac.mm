// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/l10n/l10n_util_mac.h"

#import <Foundation/Foundation.h>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "base/lazy_instance.h"
#import "base/strings/sys_string_conversions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

base::LazyInstance<std::string>::DestructorAtExit g_overridden_locale =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace l10n_util {

// Thread-safety:
//
// This function returns a reference to a global variable that is mutated by
// OverrideLocaleWithCocoaLocale(), is called from multiple thread and doesn't
// include locking.
//
// This may appear thread-unsafe. However, OverrideLocaleWithCocoaLocale() is
// only called once during the application startup before the creation of the
// Chromium threads, thus for the threads this can be considered as a constant
// and can be safely be accessed without synchronisation nor memory barrier.
//
// This is only true as long as no new usage of OverrideLocaleWithCocoaLocale()
// is added to the code base.
const std::string& GetLocaleOverride() {
  return g_overridden_locale.Get();
}

void OverrideLocaleWithCocoaLocale() {
  // NSBundle really should only be called on the main thread.
  DCHECK(NSThread.isMainThread);

  // Chrome really only has one concept of locale, but macOS has locale and
  // language that can be set independently.  After talking with Chrome UX folks
  // (Cole), the best path from an experience point of view is to map the macOS
  // language into the Chrome locale.  This way strings like "Yesterday" and
  // "Today" are in the same language as raw dates like "March 20, 1999" (Chrome
  // strings resources vs ICU generated strings).  This also makes the Mac act
  // like other Chrome platforms.
  NSArray* language_list = base::apple::OuterBundle().preferredLocalizations;
  NSString* first_locale = language_list[0];
  // macOS uses "_" instead of "-", so swap to get a real locale value.
  std::string locale_value = base::SysNSStringToUTF8(
      [first_locale stringByReplacingOccurrencesOfString:@"_" withString:@"-"]);

  // On disk the "en-US" resources are just "en" (http://crbug.com/25578), so
  // the reverse mapping is done here to continue to feed Chrome the same values
  // in all cases on all platforms.  (l10n_util maps en to en-US if it gets
  // passed this on the command line)
  if (locale_value == "en")
    locale_value = "en-US";

  g_overridden_locale.Get() = locale_value;
}

// Remove the Windows-style accelerator marker and change "..." into an
// ellipsis.  Returns the result in an autoreleased NSString.
NSString* FixUpWindowsStyleLabel(const std::u16string& label) {
  const char16_t kEllipsisUTF16 = 0x2026;
  std::u16string ret;
  size_t label_len = label.length();
  ret.reserve(label_len);
  for (size_t i = 0; i < label_len; ++i) {
    char16_t c = label[i];
    if (c == '(' && i + 3 < label_len && label[i + 1] == '&'
        && label[i + 3] == ')') {
      // Strip '(&?)' patterns which means Windows-style accelerator in some
      // non-English locales such as Japanese.
      i += 3;
    } else if (c == '&') {
      if (i + 1 < label_len && label[i + 1] == '&') {
        ret.push_back(c);
        ++i;
      }
    } else if (c == '.' && i + 2 < label_len && label[i + 1] == '.'
               && label[i + 2] == '.') {
      ret.push_back(kEllipsisUTF16);
      i += 2;
    } else {
      ret.push_back(c);
    }
  }

  return base::SysUTF16ToNSString(ret);
}

NSString* GetNSString(int message_id) {
  return base::SysUTF16ToNSString(l10n_util::GetStringUTF16(message_id));
}

NSString* GetNSStringF(int message_id, const std::u16string& a) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a));
}

NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b));
}

NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       const std::u16string& c) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b, c));
}

NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       const std::u16string& c,
                       const std::u16string& d) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b, c, d));
}

NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       const std::u16string& c,
                       const std::u16string& d,
                       const std::u16string& e) {
  return base::SysUTF16ToNSString(
      l10n_util::GetStringFUTF16(message_id, a, b, c, d, e));
}

NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       std::vector<size_t>* offsets) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(message_id,
                                                             a, b, offsets));
}

NSString* GetNSStringWithFixup(int message_id) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringUTF16(message_id));
}

NSString* GetNSStringFWithFixup(int message_id, const std::u16string& a) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const std::u16string& a,
                                const std::u16string& b) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a, b));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const std::u16string& a,
                                const std::u16string& b,
                                const std::u16string& c) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a, b, c));
}

NSString* GetNSStringFWithFixup(int message_id,
                                const std::u16string& a,
                                const std::u16string& b,
                                const std::u16string& c,
                                const std::u16string& d) {
  return FixUpWindowsStyleLabel(l10n_util::GetStringFUTF16(message_id,
                                                           a, b, c, d));
}

NSString* GetPluralNSStringF(int message_id, int number) {
  return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(message_id,
                                                                   number));
}

NSString* GetPluralNSStringFWithFixup(int message_id, int number) {
  return FixUpWindowsStyleLabel(
      l10n_util::GetPluralStringFUTF16(message_id, number));
}

}  // namespace l10n_util
