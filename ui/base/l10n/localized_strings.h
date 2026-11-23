// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions for retrieving and formatting localized strings
// from the resource bundle.

#ifndef UI_BASE_L10N_LOCALIZED_STRINGS_H_
#define UI_BASE_L10N_LOCALIZED_STRINGS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"

namespace l10n_util {

//
// Mac Note: See l10n_util_mac.h for some NSString versions and other support.
//

// Pulls resource string from the string bundle and returns it.
COMPONENT_EXPORT(UI_BASE) std::string GetStringUTF8(int message_id);
COMPONENT_EXPORT(UI_BASE) std::u16string GetStringUTF16(int message_id);

// Given a format string, replace $i with replacements[i] for all
// i < replacements.size(). Additionally, $$ is replaced by $.
// If non-NULL |offsets| will be replaced with the start points of the replaced
// strings.
COMPONENT_EXPORT(UI_BASE)
std::u16string FormatString(const std::u16string& format_string,
                            const std::vector<std::u16string>& replacements,
                            std::vector<size_t>* offsets);

// Get a resource string and replace $i with replacements[i] for all
// i < replacements.size(). Additionally, $$ is replaced by $.
// If non-NULL |offsets| will be replaced with the start points of the replaced
// strings.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::vector<std::u16string>& replacements,
                               std::vector<size_t>* offsets);

// Convenience wrappers for the above.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id, const std::u16string& a);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d,
                               const std::u16string& e);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id, const std::u16string& a);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c,
                           const std::u16string& d);

// Variants that return the offset(s) of the replaced parameters. The
// vector based version returns offsets ordered by parameter. For example if
// invoked with a and b offsets[0] gives the offset for a and offsets[1] the
// offset of b regardless of where the parameters end up in the string.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               size_t* offset);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               std::vector<size_t>* offsets);

// Convenience functions to get a string with a single integer as a parameter.
// The result will use non-ASCII(native) digits if required by a locale
// convention (e.g. Persian, Bengali).
// If a message requires plural formatting (e.g. "3 tabs open"), use
// GetPluralStringF*, instead. To format a double, integer or percentage alone
// without any surrounding text (e.g. "3.57", "123", "45%"), use
// base::Format{Double,Number,Percent}. With more than two numbers or
// number + surrounding text, use base::i18n::MessageFormatter.
// // Note that native digits have to be used in UI in general.
// base::{Int*,Double}ToString convert a number to a string with
// ASCII digits in non-UI strings.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16Int(int message_id, int a);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16Int(int message_id, int64_t a);

// Convenience functions to format a string with a single number that requires
// plural formatting. Note that a simple 2-way rule (singular vs plural)
// breaks down for a number of languages. Instead of two separate messages
// for singular and plural, use this method with one message in ICU syntax.
// See http://userguide.icu-project.org/formatparse/messages and
// go/plurals (Google internal) for more details and examples.
//
// For complex messages with input parameters of multiple types (int,
// double, time, string; e.g. "At 3:45 on Feb 3, 2016, 5 files are downloaded
// at 3 MB/s."), use base::i18n::MessageFormatter.
// message_formatter_unittest.cc also has more examples of plural formatting.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetPluralStringFUTF16(int message_id, int number);
COMPONENT_EXPORT(UI_BASE)
std::string GetPluralStringFUTF8(int message_id, int number);

// Get a string when you only care about 'single vs multiple' distinction.
// The message pointed to by |message_id| should be in ICU syntax
// (see the references above for Plural) with 'single', 'multiple', and
// 'other' (fallback) instead of 'male', 'female', and 'other' (fallback).
COMPONENT_EXPORT(UI_BASE)
std::u16string GetSingleOrMultipleStringUTF16(int message_id, bool is_multiple);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_LOCALIZED_STRINGS_H_
