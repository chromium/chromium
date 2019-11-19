// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper function for using JsTemplate. See jstemplate_builder.h for more
// info.

#include "ui/base/webui/jstemplate_builder.h"

#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/resources/grit/webui_resources.h"

namespace webui {

namespace {

// Appends a script tag with a variable name |templateData| that has the JSON
// assigned to it.
void AppendJsonHtml(const base::DictionaryValue* json, std::string* output) {
  std::string javascript_string;
  AppendJsonJS(json, &javascript_string, /*from_js_module=*/false);

  // </ confuses the HTML parser because it could be a </script> tag.  So we
  // replace </ with <\/.  The extra \ will be ignored by the JS engine.
  base::ReplaceSubstringsAfterOffset(&javascript_string, 0, "</", "<\\/");

  output->append("<script>");
  output->append(javascript_string);
  output->append("</script>");
}

// Appends the source for load_time_data.js in a script tag.
void AppendLoadTimeData(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  std::string load_time_data_src =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEBUI_JS_LOAD_TIME_DATA);

  if (load_time_data_src.empty()) {
    NOTREACHED() << "Unable to get loadTimeData src";
    return;
  }

  output->append("<script>");
  output->append(load_time_data_src);
  output->append("</script>");
}

// Appends the source for JsTemplates in a script tag.
void AppendJsTemplateSourceHtml(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  std::string jstemplate_src =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEBUI_JSTEMPLATE_JS);

  if (jstemplate_src.empty()) {
    NOTREACHED() << "Unable to get jstemplate src";
    return;
  }

  output->append("<script>");
  output->append(jstemplate_src);
  output->append("</script>");
}

// Appends the code that processes the JsTemplate with the JSON. You should
// call AppendJsTemplateSourceHtml and AppendJsonHtml before calling this.
void AppendJsTemplateProcessHtml(
    const base::StringPiece& template_id,
    std::string* output) {
  output->append("<script>");
  output->append("var tp = document.getElementById('");
  output->append(template_id.data(), template_id.size());
  output->append("');");
  output->append("jstProcess(loadTimeData.createJsEvalContext(), tp);");
  output->append("</script>");
}

}  // namespace

std::string GetI18nTemplateHtml(const base::StringPiece& html_template,
                                const base::DictionaryValue* json) {
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(*json, &replacements);
  std::string output =
      ui::ReplaceTemplateExpressions(html_template, replacements);

  AppendLoadTimeData(&output);
  AppendJsonHtml(json, &output);

  return output;
}

std::string GetTemplatesHtml(const base::StringPiece& html_template,
                             const base::DictionaryValue* json,
                             const base::StringPiece& template_id) {
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(*json, &replacements);
  std::string output =
      ui::ReplaceTemplateExpressions(html_template, replacements);

  AppendLoadTimeData(&output);
  AppendJsonHtml(json, &output);
  AppendJsTemplateSourceHtml(&output);
  AppendJsTemplateProcessHtml(template_id, &output);
  return output;
}

void AppendJsonJS(const base::DictionaryValue* json,
                  std::string* output,
                  bool from_js_module) {
  // Convert the template data to a json string.
  DCHECK(json) << "must include json data structure";

  if (from_js_module) {
    // If the script is being imported as a module, import |loadTimeData| in
    // order to allow assigning the localized strings to loadTimeData.data.
    output->append("import {loadTimeData} from ");
    output->append("'chrome://resources/js/load_time_data.m.js';\n");
  }

  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(*json);
  output->append("loadTimeData.data = ");
  output->append(jstext);
  output->append(";");
}

}  // namespace webui
