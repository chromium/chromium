// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_MAC_H_
#define UI_BASE_L10N_L10N_UTIL_MAC_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/component_export.h"

#ifdef __OBJC__
@class NSString;
#endif

namespace l10n_util {

#ifdef __OBJC__

// Remove the Windows-style accelerator marker (for labels, menuitems, etc.)
// and change "..." into an ellipsis.
// Returns the result in an autoreleased NSString.
COMPONENT_EXPORT(UI_BASE)
NSString* FixUpWindowsStyleLabel(const std::u16string& label);

// Pulls resource string from the string bundle and returns it.
COMPONENT_EXPORT(UI_BASE) NSString* GetNSString(int message_id);

// Get a resource string and replace $1-$2-$3 with |a| and |b|
// respectively.  Additionally, $$ is replaced by $.
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringF(int message_id, const std::u16string& a);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       const std::u16string& c);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       const std::u16string& c,
                       const std::u16string& d);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       const std::u16string& c,
                       const std::u16string& d,
                       const std::u16string& e);

// Variants that return the offset(s) of the replaced parameters. (See
// app/l10n_util.h for more details.)
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringF(int message_id,
                       const std::u16string& a,
                       const std::u16string& b,
                       std::vector<size_t>* offsets);

// Same as GetNSString, but runs the result through FixUpWindowsStyleLabel
// before returning it.
COMPONENT_EXPORT(UI_BASE) NSString* GetNSStringWithFixup(int message_id);

// Same as GetNSStringF, but runs the result through FixUpWindowsStyleLabel
// before returning it.
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringFWithFixup(int message_id, const std::u16string& a);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringFWithFixup(int message_id,
                                const std::u16string& a,
                                const std::u16string& b);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringFWithFixup(int message_id,
                                const std::u16string& a,
                                const std::u16string& b,
                                const std::u16string& c);
COMPONENT_EXPORT(UI_BASE)
NSString* GetNSStringFWithFixup(int message_id,
                                const std::u16string& a,
                                const std::u16string& b,
                                const std::u16string& c,
                                const std::u16string& d);

// Get a resource string using |number| with a locale-specific plural rule.
// |message_id| points to a message in the ICU syntax.
// See http://userguide.icu-project.org/formatparse/messages and
// go/plurals (Google internal).
COMPONENT_EXPORT(UI_BASE)
NSString* GetPluralNSStringF(int message_id, int number);

// Same as GetPluralNSStringF, but runs the result through
// FixUpWindowsStyleLabel before returning it.
COMPONENT_EXPORT(UI_BASE)
NSString* GetPluralNSStringFWithFixup(int message_id, int number);

#endif  // __OBJC__

// Support the override of the locale with the value from Cocoa.
COMPONENT_EXPORT(UI_BASE) void OverrideLocaleWithCocoaLocale();
COMPONENT_EXPORT(UI_BASE) const std::string& GetLocaleOverride();

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_MAC_H_
