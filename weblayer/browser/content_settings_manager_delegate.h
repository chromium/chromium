// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONTENT_SETTINGS_MANAGER_DELEGATE_H_
#define WEBLAYER_BROWSER_CONTENT_SETTINGS_MANAGER_DELEGATE_H_

#include "components/content_settings/browser/content_settings_manager_impl.h"

namespace weblayer {

class ContentSettingsManagerDelegate
    : public content_settings::ContentSettingsManagerImpl::Delegate {
 public:
  ContentSettingsManagerDelegate();
  ~ContentSettingsManagerDelegate() override;

 private:
  // content_settings::ContentSettingsManagerImpl::Delegate:
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override;
  bool AllowStorageAccess(
      int render_process_id,
      int render_frame_id,
      content_settings::mojom::ContentSettingsManager::StorageType storage_type,
      const GURL& url,
      bool allowed,
      base::OnceCallback<void(bool)>* callback) override;
  std::unique_ptr<Delegate> Clone() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONTENT_SETTINGS_MANAGER_DELEGATE_H_
