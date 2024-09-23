// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WEBUI_WEB_UI_UTIL_H_
#define UI_BASE_WEBUI_WEB_UI_UTIL_H_

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/values.h"
#include "ui/base/template_expressions.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class SkBitmap;

namespace webui {

struct LocalizedString {
  const char* name;
  int id;
};

// Convenience routine to convert SkBitmap object to data url
// so that it can be used in WebUI.
COMPONENT_EXPORT(UI_BASE) std::string GetBitmapDataUrl(const SkBitmap& bitmap);

// Convenience routine to convert an in-memory PNG to a data url for WebUI use.
COMPONENT_EXPORT(UI_BASE)
std::string GetPngDataUrl(const unsigned char* data, size_t size);

// Extracts a disposition from click event arguments. |args| should contain
// an integer button and booleans alt key, ctrl key, meta key, and shift key
// (in that order), starting at |start_index|.
COMPONENT_EXPORT(UI_BASE)
WindowOpenDisposition GetDispositionFromClick(const base::Value::List& args,
                                              size_t start_index);

// Parse a formatted scale factor string into float and sets to |scale_factor|.
COMPONENT_EXPORT(UI_BASE)
bool ParseScaleFactor(std::string_view identifier, float* scale_factor);

// Parses a URL containing some path [{frame}]@{scale}x. If it contains a
// scale factor then it is returned and the associated part of the URL is
// removed from the returned  |path|, otherwise the default scale factor is
// returned and |path| is left intact. If it contains a frame index then it
// is returned and the associated part of the URL is removed from the
// returned |path|, otherwise the default frame index is returned and |path|
// is left intact.
COMPONENT_EXPORT(UI_BASE)
void ParsePathAndImageSpec(const GURL& url,
                           std::string* path,
                           float* scale_factor,
                           int* frame_index);

// Helper function to set some default values (e.g., font family, size,
// language, and text direction) into the given dictionary. Requires an
// application locale (i.e. g_browser_process->GetApplicationLocale()).
COMPONENT_EXPORT(UI_BASE)
void SetLoadTimeDataDefaults(const std::string& app_locale,
                             base::Value::Dict* localized_strings);

COMPONENT_EXPORT(UI_BASE)
void SetLoadTimeDataDefaults(const std::string& app_locale,
                             ui::TemplateReplacements* replacements);

// Get a CSS declaration for common text styles for all of Web UI.
COMPONENT_EXPORT(UI_BASE) std::string GetWebUiCssTextDefaults();

// Appends the CSS declaration returned by GetWebUiCssTextDefaults() as an
// inline stylesheet.
COMPONENT_EXPORT(UI_BASE) void AppendWebUiCssTextDefaults(std::string* html);

// Get some common font styles for all of WebUI.
COMPONENT_EXPORT(UI_BASE) std::string GetFontFamily();
COMPONENT_EXPORT(UI_BASE) std::string GetFontSize();
COMPONENT_EXPORT(UI_BASE) std::string GetTextDirection();

// A helper function that generates a string of HTML to be loaded. The returned
// string has all $i8n{...} placeholders replaced with localized strings and
// also includes an script that injects `loadTimeDataRaw` in the global `window`
// scope.
COMPONENT_EXPORT(UI_BASE)
std::string GetLocalizedHtml(std::string_view html_template,
                             const base::Value::Dict& strings);
}  // namespace webui

#endif  // UI_BASE_WEBUI_WEB_UI_UTIL_H_
