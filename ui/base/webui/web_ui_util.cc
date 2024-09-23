// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/webui/web_ui_util.h"

#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/i18n/rtl.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/app_locale_settings.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace webui {
namespace {

// Generous cap to guard against out-of-memory issues.
constexpr float kMaxScaleFactor = 1000.0f;

std::string GetFontFamilyMd() {
#if BUILDFLAG(IS_LINUX)
  return "Roboto, " + GetFontFamily();
#else
  return GetFontFamily();
#endif
}

std::string GetWebUiCssTextDefaults(const std::string& css_template) {
  ui::TemplateReplacements placeholders;
  placeholders["textdirection"] = GetTextDirection();
  placeholders["fontfamily"] = GetFontFamily();
  placeholders["fontfamilyMd"] = GetFontFamilyMd();
  placeholders["fontsize"] = GetFontSize();
  return ui::ReplaceTemplateExpressions(css_template, placeholders);
}

}  // namespace

std::string GetBitmapDataUrl(const SkBitmap& bitmap) {
  TRACE_EVENT2("ui", "GetBitmapDataUrl", "width", bitmap.width(), "height",
               bitmap.height());
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  return GetPngDataUrl(output.data(), output.size());
}

std::string GetPngDataUrl(const unsigned char* data, size_t size) {
  std::string output = "data:image/png;base64,";
  base::Base64EncodeAppend(base::make_span(data, size), &output);
  return output;
}

WindowOpenDisposition GetDispositionFromClick(const base::Value::List& list,
                                              size_t start_index) {
  double button = list[start_index].GetDouble();
  bool alt_key = list[start_index + 1].GetBool();
  bool ctrl_key = list[start_index + 2].GetBool();
  bool meta_key = list[start_index + 3].GetBool();
  bool shift_key = list[start_index + 4].GetBool();

  return ui::DispositionFromClick(
      button == 1.0, alt_key, ctrl_key, meta_key, shift_key);
}

bool ParseScaleFactor(std::string_view identifier, float* scale_factor) {
  *scale_factor = 1.0f;
  if (identifier.empty()) {
    DLOG(WARNING) << "Invalid scale factor format: " << identifier;
    return false;
  }

  if (*identifier.rbegin() != 'x') {
    DLOG(WARNING) << "Invalid scale factor format: " << identifier;
    return false;
  }

  double scale = 0;
  std::string stripped(identifier.substr(0, identifier.length() - 1));
  if (!base::StringToDouble(stripped, &scale)) {
    DLOG(WARNING) << "Invalid scale factor format: " << identifier;
    return false;
  }
  if (scale <= 0) {
    DLOG(WARNING) << "Invalid non-positive scale factor: " << identifier;
    return false;
  }
  if (scale > kMaxScaleFactor) {
    DLOG(WARNING) << "Invalid scale factor, too large: " << identifier;
    return false;
  }
  *scale_factor = static_cast<float>(scale);
  return true;
}

// Parse a formatted frame index string into int and sets to |frame_index|.
bool ParseFrameIndex(std::string_view identifier, int* frame_index) {
  *frame_index = -1;
  if (identifier.empty()) {
    DLOG(WARNING) << "Invalid frame index format: " << identifier;
    return false;
  }

  if (*identifier.rbegin() != ']') {
    DLOG(WARNING) << "Invalid frame index format: " << identifier;
    return false;
  }

  unsigned frame = 0;
  if (!base::StringToUint(identifier.substr(0, identifier.length() - 1),
                          &frame)) {
    DLOG(WARNING) << "Invalid frame index format: " << identifier;
    return false;
  }
  *frame_index = static_cast<int>(frame);
  return true;
}

void ParsePathAndImageSpec(const GURL& url,
                           std::string* path,
                           float* scale_factor,
                           int* frame_index) {
  *path = base::UnescapeBinaryURLComponent(url.path_piece().substr(1));
  if (scale_factor)
    *scale_factor = 1.0f;
  if (frame_index)
    *frame_index = -1;

  // Detect and parse resource string ending in @<scale>x.
  std::size_t pos = path->rfind('@');
  if (pos != std::string::npos) {
    std::string_view stripped_path(*path);
    float factor;

    if (ParseScaleFactor(stripped_path.substr(
            pos + 1, stripped_path.length() - pos - 1), &factor)) {
      // Strip scale factor specification from path.
      stripped_path.remove_suffix(stripped_path.length() - pos);
      *path = std::string(stripped_path);
    }
    if (scale_factor)
      *scale_factor = factor;
  }

  // Detect and parse resource string ending in [<frame>].
  pos = path->rfind('[');
  if (pos != std::string::npos) {
    std::string_view stripped_path(*path);
    int index;

    if (ParseFrameIndex(
            stripped_path.substr(pos + 1, stripped_path.length() - pos - 1),
            &index)) {
      // Strip frame index specification from path.
      stripped_path.remove_suffix(stripped_path.length() - pos);
      *path = std::string(stripped_path);
    }
    if (frame_index)
      *frame_index = index;
  }
}

void SetLoadTimeDataDefaults(const std::string& app_locale,
                             base::Value::Dict* localized_strings) {
  localized_strings->Set("fontfamily", GetFontFamily());
  localized_strings->Set("fontfamilyMd", GetFontFamilyMd());
  localized_strings->Set("fontsize", GetFontSize());
  localized_strings->Set("language", l10n_util::GetLanguage(app_locale));
  localized_strings->Set("textdirection", GetTextDirection());
}

void SetLoadTimeDataDefaults(const std::string& app_locale,
                             ui::TemplateReplacements* replacements) {
  (*replacements)["fontfamily"] = GetFontFamily();
  (*replacements)["fontfamilyMd"] = GetFontFamilyMd();
  (*replacements)["fontsize"] = GetFontSize();
  (*replacements)["language"] = l10n_util::GetLanguage(app_locale);
  (*replacements)["textdirection"] = GetTextDirection();
}

std::string GetWebUiCssTextDefaults() {
  const ui::ResourceBundle& resource_bundle =
      ui::ResourceBundle::GetSharedInstance();
  return GetWebUiCssTextDefaults(
      resource_bundle.LoadDataResourceString(IDR_WEBUI_CSS_TEXT_DEFAULTS_CSS));
}

void AppendWebUiCssTextDefaults(std::string* html) {
  html->append("<style>");
  html->append(GetWebUiCssTextDefaults());
  html->append("</style>");
}

std::string GetFontFamily() {
  std::string font_family = l10n_util::GetStringUTF8(IDS_WEB_FONT_FAMILY);

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  std::string font_name = ui::ResourceBundle::GetSharedInstance()
                              .GetFont(ui::ResourceBundle::BaseFont)
                              .GetFontName();
  // Wrap |font_name| with quotes to ensure it will always be parsed correctly
  // in CSS.
  font_family = "\"" + font_name + "\", " + font_family;
#endif

  return font_family;
}

std::string GetFontSize() {
  return l10n_util::GetStringUTF8(IDS_WEB_FONT_SIZE);
}

std::string GetTextDirection() {
  return base::i18n::IsRTL() ? "rtl" : "ltr";
}

std::string GetLocalizedHtml(std::string_view html_template,
                             const base::Value::Dict& strings) {
  // Populate $i18n{...} placeholders.
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(strings, &replacements);
  std::string output =
      ui::ReplaceTemplateExpressions(html_template, replacements);

  // Inject data to the UI that will be used to populate loadTimeData upon
  // initialization.
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(strings);
  output.append("<script>");
  output.append("var loadTimeDataRaw = ");
  output.append(json);
  output.append(";");
  output.append("</script>");

  return output;
}

}  // namespace webui
