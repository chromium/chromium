// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides some helper methods for building and rendering an
// internal html page.  The flow is as follows:
// - instantiate a builder given a webframe that we're going to render content
//   into
// - load the template html and load the jstemplate javascript into the frame
// - given a json data object, run the jstemplate javascript which fills in
//   template values

#ifndef UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_
#define UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_

#include <string>

#include "base/strings/string_piece.h"
#include "ui/base/ui_base_export.h"

namespace base {
class DictionaryValue;
}

namespace webui {

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for i18n Templates.
UI_BASE_EXPORT std::string GetI18nTemplateHtml(
    const base::StringPiece& html_template,
    const base::DictionaryValue* json);

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for both i18n Templates and JsTemplates.
UI_BASE_EXPORT std::string GetTemplatesHtml(
    const base::StringPiece& html_template,
    const base::DictionaryValue* json,
    const base::StringPiece& template_id);

// Assigns the given json data into |loadTimeData|, without a <script> tag.
UI_BASE_EXPORT void AppendJsonJS(const base::DictionaryValue* json,
                                 std::string* output,
                                 bool from_js_module);

}  // namespace webui

#endif  // UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_
