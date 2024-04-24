// Copyright 2013 The Chromium Authors
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
#include <string_view>

#include "base/component_export.h"
#include "base/values.h"

namespace webui {

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for i18n Templates.
COMPONENT_EXPORT(UI_BASE)
std::string GetI18nTemplateHtml(std::string_view html_template,
                                const base::Value::Dict& json);

// Assigns the given json data into |loadTimeData|, without a <script> tag.
COMPONENT_EXPORT(UI_BASE)
void AppendJsonJS(const base::Value::Dict& json,
                  std::string* output,
                  bool from_js_module);

}  // namespace webui

#endif  // UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_
