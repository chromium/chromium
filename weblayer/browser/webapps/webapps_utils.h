// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBAPPS_WEBAPPS_UTILS_H_
#define WEBLAYER_BROWSER_WEBAPPS_WEBAPPS_UTILS_H_

#include <string>

class GURL;
class SkBitmap;

namespace webapps {

// Adds a shortcut to the home screen. Calls the native method
// addShortcutToHomescreen in WebappsHelper.java
void addShortcutToHomescreen(const std::string& id,
                             const GURL& url,
                             const std::u16string& user_title,
                             const SkBitmap& primary_icon,
                             bool is_primary_icon_maskable);

}  // namespace webapps

#endif  // WEBLAYER_BROWSER_WEBAPPS_WEBAPPS_UTILS_H_
