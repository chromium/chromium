// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/browser/safe_browsing_network_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "weblayer/browser/safe_browsing/url_checker_delegate_impl.h"

namespace weblayer {

namespace {

network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams(
    const std::string& user_agent) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->user_agent = user_agent;
  return network_context_params;
}

}  // namespace

SafeBrowsingService::SafeBrowsingService(const std::string& user_agent)
    : user_agent_(user_agent) {}

SafeBrowsingService::~SafeBrowsingService() {}

void SafeBrowsingService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (network_context_) {
    // already initialized
    return;
  }

  safe_browsing_api_handler_.reset(
      new safe_browsing::SafeBrowsingApiHandlerBridge());
  safe_browsing::SafeBrowsingApiHandler::SetInstance(
      safe_browsing_api_handler_.get());

  base::FilePath user_data_dir;
  bool result =
      base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir);
  DCHECK(result);

  // safebrowsing network context needs to be created on the UI thread.
  network_context_ =
      std::make_unique<safe_browsing::SafeBrowsingNetworkContext>(
          user_data_dir,
          base::BindRepeating(CreateDefaultNetworkContextParams, user_agent_));

  CreateSafeBrowsingUIManager();
}

std::unique_ptr<blink::URLLoaderThrottle>
SafeBrowsingService::CreateURLLoaderThrottle(
    content::ResourceContext* resource_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return safe_browsing::BrowserURLLoaderThrottle::Create(
      base::BindOnce(
          [](SafeBrowsingService* sb_service, content::ResourceContext*) {
            return sb_service->GetSafeBrowsingUrlCheckerDelegate();
          },
          base::Unretained(this)),
      wc_getter, frame_tree_node_id, resource_context);
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
SafeBrowsingService::GetSafeBrowsingUrlCheckerDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ = new UrlCheckerDelegateImpl(
        GetSafeBrowsingDBManager(), GetSafeBrowsingUIManager());
  }

  return safe_browsing_url_checker_delegate_;
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
SafeBrowsingService::GetSafeBrowsingDBManager() {
  if (!safe_browsing_db_manager_) {
    CreateAndStartSafeBrowsingDBManager();
  }
  return safe_browsing_db_manager_.get();
}

SafeBrowsingUIManager* SafeBrowsingService::GetSafeBrowsingUIManager() {
  return ui_manager_.get();
}

void SafeBrowsingService::CreateSafeBrowsingUIManager() {
  DCHECK(!ui_manager_);
  ui_manager_ = new SafeBrowsingUIManager();
}

void SafeBrowsingService::CreateAndStartSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!safe_browsing_db_manager_);

  safe_browsing_db_manager_ =
      new safe_browsing::RemoteSafeBrowsingDatabaseManager();

  // V4ProtocolConfig is not used. Just create one with empty values.
  safe_browsing::V4ProtocolConfig config("", false, "", "");
  safe_browsing_db_manager_->StartOnIOThread(GetURLLoaderFactoryOnIOThread(),
                                             config);
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingService::GetURLLoaderFactoryOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!shared_url_loader_factory_on_io_) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&SafeBrowsingService::CreateURLLoaderFactoryForIO,
                       base::Unretained(this),
                       MakeRequest(&url_loader_factory_on_io_)));
    shared_url_loader_factory_on_io_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_on_io_.get());
  }
  return shared_url_loader_factory_on_io_;
}

void SafeBrowsingService::CreateURLLoaderFactoryForIO(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_corb_enabled = false;
  network_context_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(receiver), std::move(url_loader_factory_params));
}

}  // namespace weblayer
