// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_fetch/background_fetch_delegate_impl.h"

#include <utility>

#include "base/check_op.h"
#include "components/background_fetch/download_client.h"
#include "components/background_fetch/job_details.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/download_service_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

BackgroundFetchDelegateImpl::BackgroundFetchDelegateImpl(
    content::BrowserContext* context)
    : background_fetch::BackgroundFetchDelegateBase(context) {}

BackgroundFetchDelegateImpl::~BackgroundFetchDelegateImpl() = default;

void BackgroundFetchDelegateImpl::UpdateUI(
    const std::string& job_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(title || icon);             // One of the UI options must be updatable.
  DCHECK(!icon || !icon->isNull());  // The |icon|, if provided, is not null.

  background_fetch::JobDetails* job_details =
      GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details)
    return;

  if (title)
    job_details->fetch_description->title = *title;

  if (icon)
    job_details->fetch_description->icon = *icon;

  DoUpdateUi(job_id);

  if (auto client = GetClient(job_id))
    client->OnUIUpdated(job_id);
}

void BackgroundFetchDelegateImpl::GetPermissionForOriginWithoutWebContents(
    const url::Origin& origin,
    GetPermissionForOriginCallback callback) {
  // TODO(estade): handle the case where there's no WebContents.
  std::move(callback).Run(content::BackgroundFetchPermission::BLOCKED);
}

download::DownloadService* BackgroundFetchDelegateImpl::GetDownloadService() {
  return DownloadServiceFactory::GetForBrowserContext(context());
}

void BackgroundFetchDelegateImpl::OnDownloadStartedForJob(
    const std::string& job_id) {
  // Create the UI the first time a download starts. (There may be multiple
  // downloads for a single background fetch job.)
  background_fetch::JobDetails* job = GetJobDetails(job_id);
  auto inserted = ui_item_map_.emplace(
      std::piecewise_construct, std::forward_as_tuple(job_id),
      std::forward_as_tuple(this, job_id, job));
  if (inserted.second) {
    ProfileImpl::FromBrowserContext(context())
        ->download_delegate()
        ->DownloadStarted(&inserted.first->second);
  } else {
    DoUpdateUi(job_id);
  }
}

void BackgroundFetchDelegateImpl::DoUpdateUi(const std::string& job_id) {
  auto iter = ui_item_map_.find(job_id);
  if (iter == ui_item_map_.end())
    return;

  BackgroundFetchDownload* download = &iter->second;

  if (!download->HasBeenAddedToUi())
    return;

  ProfileImpl::FromBrowserContext(context())
      ->download_delegate()
      ->DownloadProgressChanged(download);
}

void BackgroundFetchDelegateImpl::DoCleanUpUi(const std::string& job_id) {
  ui_item_map_.erase(job_id);
}

}  // namespace weblayer
