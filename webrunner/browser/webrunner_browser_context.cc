// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/webrunner_browser_context.h"

#include <memory>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/network_switches.h"
#include "webrunner/browser/webrunner_net_log.h"
#include "webrunner/browser/webrunner_url_request_context_getter.h"
#include "webrunner/service/common.h"

namespace webrunner {

class WebRunnerBrowserContext::ResourceContext
    : public content::ResourceContext {
 public:
  ResourceContext() = default;
  ~ResourceContext() override = default;

  // ResourceContext implementation.
  net::URLRequestContext* GetRequestContext() override {
    DCHECK(getter_);
    return getter_->GetURLRequestContext();
  }

  void set_url_request_context_getter(
      scoped_refptr<WebRunnerURLRequestContextGetter> getter) {
    getter_ = std::move(getter);
  }

 private:
  scoped_refptr<WebRunnerURLRequestContextGetter> getter_;

  DISALLOW_COPY_AND_ASSIGN(ResourceContext);
};

std::unique_ptr<WebRunnerNetLog> CreateNetLog() {
  std::unique_ptr<WebRunnerNetLog> result;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(network::switches::kLogNetLog)) {
    base::FilePath log_path =
        command_line->GetSwitchValuePath(network::switches::kLogNetLog);
    result = std::make_unique<WebRunnerNetLog>(log_path);
  }

  return result;
}

WebRunnerBrowserContext::WebRunnerBrowserContext(bool force_incognito)
    : net_log_(CreateNetLog()), resource_context_(new ResourceContext()) {
  if (!force_incognito) {
    base::PathService::Get(base::DIR_APP_DATA, &data_dir_path_);
    if (!base::PathExists(data_dir_path_)) {
      // Run in incognito mode if /data doesn't exist.
      data_dir_path_.clear();
    }
  }

  BrowserContext::Initialize(this, data_dir_path_);
}

WebRunnerBrowserContext::~WebRunnerBrowserContext() {
  NotifyWillBeDestroyed(this);

  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       std::move(resource_context_));
  }

  ShutdownStoragePartitions();
}

std::unique_ptr<content::ZoomLevelDelegate>
WebRunnerBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath WebRunnerBrowserContext::GetPath() const {
  return data_dir_path_;
}

base::FilePath WebRunnerBrowserContext::GetCachePath() const {
  NOTIMPLEMENTED();
  return base::FilePath();
}

bool WebRunnerBrowserContext::IsOffTheRecord() const {
  return data_dir_path_.empty();
}

content::ResourceContext* WebRunnerBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
WebRunnerBrowserContext::GetDownloadManagerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

content::BrowserPluginGuestManager* WebRunnerBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
WebRunnerBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService*
WebRunnerBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate*
WebRunnerBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
WebRunnerBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
WebRunnerBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
WebRunnerBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
WebRunnerBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* WebRunnerBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!url_request_getter_);
  url_request_getter_ = new WebRunnerURLRequestContextGetter(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      net_log_.get(), std::move(*protocol_handlers),
      std::move(request_interceptors), data_dir_path_);
  resource_context_->set_url_request_context_getter(url_request_getter_);
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
WebRunnerBrowserContext::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

net::URLRequestContextGetter*
WebRunnerBrowserContext::CreateMediaRequestContext() {
  DCHECK(url_request_getter_.get());
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
WebRunnerBrowserContext::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return nullptr;
}

}  // namespace webrunner
