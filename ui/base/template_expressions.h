// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions for replacing template expressions.
// For example "Hello ${name}" could have ${name} replaced by the user's name.

#ifndef UI_BASE_TEMPLATE_EXPRESSIONS_H_
#define UI_BASE_TEMPLATE_EXPRESSIONS_H_

#include <map>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/values.h"

namespace ui {

// Map of strings for template replacement in |ReplaceTemplateExpressions|.
typedef std::map<const std::string, std::string> TemplateReplacements;

// Convert a dictionary to a replacement map. This helper function is to assist
// migration to using TemplateReplacements directly (which is preferred).
// TODO(dschuyler): remove this function by using TemplateReplacements directly.
COMPONENT_EXPORT(UI_BASE)
void TemplateReplacementsFromDictionaryValue(
    const base::Value::Dict& dictionary,
    TemplateReplacements* replacements);

// Replace $i18n*{foo} in the format string with the value for the foo key in
// |replacements|.  If the key is not found in the |replacements| that item will
// be unaltered.
COMPONENT_EXPORT(UI_BASE)
std::string ReplaceTemplateExpressions(
    std::string_view source,
    const TemplateReplacements& replacements,
    bool skip_unexpected_placeholder_check = false);

// Replace $i18n*{foo} in the HTML template contained in |source| with the
// value for the foo key in |replacements| and return the result in |output|.
// Only $i18n*{...} expressions in the HTML portion of the JS source will be
// replaced; such expressions in the rest of the JS code will be left unaltered.
// If no template is found, |source| will be returned in |output| unaltered. If
// a key is not found in the |replacements| that item will be unaltered. Returns
// true on success, false on failure. On failure, |output| will not populated.
// Replacement will fail if a single HTML template string cannot be identified
// (e.g. not terminated, multiple _template: html`... in a single file), or if
// executing the replacements would be unsafe (e.g. result in unescaped
// backticks or "${" within the HTML string).
// Note: Currently, this only supports the legacy Polymer syntax, i.e.:
//     _template: html` ... `,
COMPONENT_EXPORT(UI_BASE)
bool ReplaceTemplateExpressionsInJS(std::string_view source,
                                    const TemplateReplacements& replacements,
                                    std::string* output);

}  // namespace ui

#endif  // UI_BASE_TEMPLATE_EXPRESSIONS_H_
