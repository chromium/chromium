// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WEBUI_WEB_UI_UTIL_H_
#define UI_BASE_WEBUI_WEB_UI_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "ui/base/template_expressions.h"
#include "ui/base/ui_base_export.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class SkBitmap;

namespace webui {

// Convenience routine to convert SkBitmap object to data url
// so that it can be used in WebUI.
UI_BASE_EXPORT std::string GetBitmapDataUrl(const SkBitmap& bitmap);

// Convenience routine to convert an in-memory PNG to a data url for WebUI use.
UI_BASE_EXPORT std::string GetPngDataUrl(const unsigned char* data,
                                         size_t size);

// Extracts a disposition from click event arguments. |args| should contain
// an integer button and booleans alt key, ctrl key, meta key, and shift key
// (in that order), starting at |start_index|.
UI_BASE_EXPORT WindowOpenDisposition
    GetDispositionFromClick(const base::ListValue* args, int start_index);

// Parse a formatted scale factor string into float and sets to |scale_factor|.
UI_BASE_EXPORT bool ParseScaleFactor(const base::StringPiece& identifier,
                                     float* scale_factor);

// Parses a URL containing some path [{frame}]@{scale}x. If it contains a
// scale factor then it is returned and the associated part of the URL is
// removed from the returned  |path|, otherwise the default scale factor is
// returned and |path| is left intact. If it contains a frame index then it
// is returned and the associated part of the URL is removed from the
// returned |path|, otherwise the default frame index is returned and |path|
// is left intact.
UI_BASE_EXPORT void ParsePathAndImageSpec(const GURL& url,
                                          std::string* path,
                                          float* scale_factor,
                                          int* frame_index);

// Parses a URL containing some path @{scale}x. If it does not contain a scale
// factor then the default scale factor is returned.
UI_BASE_EXPORT void ParsePathAndScale(const GURL& url,
                                      std::string* path,
                                      float* scale_factor);

// Parses a URL containing some path [{frame}]. If it does not contain a frame
// index then the default frame index is returned.
UI_BASE_EXPORT void ParsePathAndFrame(const GURL& url,
                                      std::string* path,
                                      int* frame_index);

// Helper function to set some default values (e.g., font family, size,
// language, and text direction) into the given dictionary. Requires an
// application locale (i.e. g_browser_process->GetApplicationLocale()).
UI_BASE_EXPORT void SetLoadTimeDataDefaults(
    const std::string& app_locale,
    base::DictionaryValue* localized_strings);
UI_BASE_EXPORT void SetLoadTimeDataDefaults(
    const std::string& app_locale,
    ui::TemplateReplacements* replacements);

// Get a CSS declaration for common text styles for all of Web UI.
UI_BASE_EXPORT std::string GetWebUiCssTextDefaults();

// Get a CSS declaration for common text styles for Web UI using
// Material Design.
UI_BASE_EXPORT std::string GetWebUiCssTextDefaultsMd();

// Appends the CSS declaration returned by GetWebUiCssTextDefaults() as an
// inline stylesheet.
UI_BASE_EXPORT void AppendWebUiCssTextDefaults(std::string* html);

UI_BASE_EXPORT std::string GetA11yEnhanced();
// Get some common font styles for all of WebUI.
UI_BASE_EXPORT std::string GetFontFamily();
UI_BASE_EXPORT std::string GetFontSize();
UI_BASE_EXPORT std::string GetTextDirection();

}  // namespace webui

#endif  // UI_BASE_WEBUI_WEB_UI_UTIL_H_
