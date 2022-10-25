// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_STRINGS_NETWORK_NETWORK_ELEMENT_LOCALIZED_STRINGS_PROVIDER_H_
#define UI_CHROMEOS_STRINGS_NETWORK_NETWORK_ELEMENT_LOCALIZED_STRINGS_PROVIDER_H_

namespace login {
class LocalizedValuesBuilder;
}

namespace content {
class WebUIDataSource;
}

namespace ui {
namespace network_element {

// This file contains functions to add localized strings used by
// components in ash/webui/common/resources/network/.

// Adds the strings needed for network elements to |html_source|.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

// Same as AddLocalizedStrings but for a LocalizedValuesBuilder.
void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder);

// Adds ONC strings used by the details dialog used in Settings and WebUI.
void AddOncLocalizedStrings(content::WebUIDataSource* html_source);

// Adds strings used by the details dialog used in Settings and WebUI.
void AddDetailsLocalizedStrings(content::WebUIDataSource* html_source);

// Adds strings used by the configuration dialog used in Settings and
// WebUI.
void AddConfigLocalizedStrings(content::WebUIDataSource* html_source);

// Adds error strings for networking UI.
void AddErrorLocalizedStrings(content::WebUIDataSource* html_source);

}  // namespace network_element
}  // namespace ui

#endif  // UI_CHROMEOS_STRINGS_NETWORK_NETWORK_ELEMENT_LOCALIZED_STRINGS_PROVIDER_H_
