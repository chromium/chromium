// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_settings_manager_delegate.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "weblayer/browser/cookie_settings_factory.h"

namespace weblayer {

ContentSettingsManagerDelegate::ContentSettingsManagerDelegate() = default;

ContentSettingsManagerDelegate::~ContentSettingsManagerDelegate() = default;

scoped_refptr<content_settings::CookieSettings>
ContentSettingsManagerDelegate::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return CookieSettingsFactory::GetForBrowserContext(browser_context);
}

bool ContentSettingsManagerDelegate::AllowStorageAccess(
    int render_process_id,
    int render_frame_id,
    content_settings::mojom::ContentSettingsManager::StorageType storage_type,
    const GURL& url,
    bool allowed,
    base::OnceCallback<void(bool)>* callback) {
  return false;
}

std::unique_ptr<content_settings::ContentSettingsManagerImpl::Delegate>
ContentSettingsManagerDelegate::Clone() {
  return std::make_unique<ContentSettingsManagerDelegate>();
}

}  // namespace weblayer
