// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/browser_context.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"

namespace webui_examples {

BrowserContext::BrowserContext(const base::FilePath& temp_dir_path)
    : temp_dir_path_(temp_dir_path),
      resource_context_(std::make_unique<content::ResourceContext>()) {}

BrowserContext::~BrowserContext() {
  NotifyWillBeDestroyed();
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                     resource_context_.release());
  ShutdownStoragePartitions();
}

// Creates a delegate to initialize a HostZoomMap and persist its information.
// This is called during creation of each StoragePartition.
std::unique_ptr<content::ZoomLevelDelegate>
BrowserContext::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath BrowserContext::GetPath() {
  return temp_dir_path_;
}

bool BrowserContext::IsOffTheRecord() {
  return false;
}

content::ResourceContext* BrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate* BrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager* BrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* BrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PlatformNotificationService*
BrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* BrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
BrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* BrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
BrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

content::ReduceAcceptLanguageControllerDelegate*
BrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

content::ClientHintsControllerDelegate*
BrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate* BrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
BrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
BrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

}  // namespace webui_examples
