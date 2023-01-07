// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/devtools_manager_delegate_android.h"

#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

DevToolsManagerDelegateAndroid::DevToolsManagerDelegateAndroid() = default;

DevToolsManagerDelegateAndroid::~DevToolsManagerDelegateAndroid() = default;

content::BrowserContext*
DevToolsManagerDelegateAndroid::GetDefaultBrowserContext() {
  auto profiles = ProfileImpl::GetAllProfiles();
  if (profiles.empty())
    return nullptr;

  // This is called when granting permissions via devtools in browser tests or WPT. We assume that
  // there is only a single profile and just pick the first one here. Note that outside of tests
  // there might exist multiple profiles for WebLayer and this assumption won't hold.
  ProfileImpl* profile = *profiles.begin();
  return profile->GetBrowserContext();
}

std::string DevToolsManagerDelegateAndroid::GetDiscoveryPageHTML() {
  const char html[] =
      "<html>"
      "<head><title>WebLayer remote debugging</title></head>"
      "<body>Please use <a href=\'chrome://inspect\'>chrome://inspect</a>"
      "</body>"
      "</html>";
  return html;
}

bool DevToolsManagerDelegateAndroid::IsBrowserTargetDiscoverable() {
  return true;
}

}  // namespace weblayer
