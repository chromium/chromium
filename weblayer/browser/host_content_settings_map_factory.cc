// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/host_content_settings_map_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace weblayer {

HostContentSettingsMapFactory::HostContentSettingsMapFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "HostContentSettingsMap",
          BrowserContextDependencyManager::GetInstance()) {}

HostContentSettingsMapFactory::~HostContentSettingsMapFactory() = default;

// static
HostContentSettingsMap* HostContentSettingsMapFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  return static_cast<HostContentSettingsMap*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true).get());
}

// static
HostContentSettingsMapFactory* HostContentSettingsMapFactory::GetInstance() {
  return base::Singleton<HostContentSettingsMapFactory>::get();
}

scoped_refptr<RefcountedKeyedService>
HostContentSettingsMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<HostContentSettingsMap> settings_map =
      base::MakeRefCounted<HostContentSettingsMap>(
          user_prefs::UserPrefs::Get(context), context->IsOffTheRecord(),
          /*store_last_modified=*/true,
          /*restore_session=*/false,
          /*should_record_metrics=*/!context->IsOffTheRecord());

  return settings_map;
}

content::BrowserContext* HostContentSettingsMapFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
