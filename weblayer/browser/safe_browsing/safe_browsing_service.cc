// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_throttle.h"
#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/url_checker_delegate_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_safe_browsing_blocking_page_factory.h"
#include "weblayer/browser/safe_browsing/weblayer_ui_manager_delegate.h"
#include "weblayer/common/features.h"

namespace weblayer {

namespace {

network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams(
    const std::string& user_agent) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->cert_verifier_params = content::GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  network_context_params->user_agent = user_agent;
  return network_context_params;
}

// Helper method that checks the RenderProcessHost is still alive and checks the
// latest Safe Browsing pref value on the UI thread before hopping over to the
// IO thread.
void MaybeCreateSafeBrowsing(
    int rph_id,
    base::WeakPtr<content::ResourceContext> resource_context,
    base::RepeatingCallback<scoped_refptr<safe_browsing::UrlCheckerDelegate>()>
        get_checker_delegate,
    mojo::PendingReceiver<safe_browsing::mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(rph_id);
  if (!render_process_host) {
    return;
  }

  bool is_safe_browsing_enabled = safe_browsing::IsSafeBrowsingEnabled(
      *static_cast<BrowserContextImpl*>(
           render_process_host->GetBrowserContext())
           ->pref_service());

  if (!is_safe_browsing_enabled) {
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&safe_browsing::MojoSafeBrowsingImpl::MaybeCreate, rph_id,
                     std::move(resource_context),
                     std::move(get_checker_delegate), std::move(receiver)));
}

}  // namespace

SafeBrowsingService::SafeBrowsingService(const std::string& user_agent)
    : user_agent_(user_agent) {}

SafeBrowsingService::~SafeBrowsingService() = default;

void SafeBrowsingService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (network_context_) {
    // already initialized
    return;
  }

  base::FilePath user_data_dir;
  bool result =
      base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir);
  DCHECK(result);

  // safebrowsing network context needs to be created on the UI thread.
  network_context_ =
      std::make_unique<safe_browsing::SafeBrowsingNetworkContext>(
          user_data_dir, /*trigger_migration=*/false,
          base::BindRepeating(CreateDefaultNetworkContextParams, user_agent_));

  CreateSafeBrowsingUIManager();

  // Needs to happen after |ui_manager_| is created.
  CreateTriggerManager();
}

std::unique_ptr<blink::URLLoaderThrottle>
SafeBrowsingService::CreateURLLoaderThrottle(
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    int frame_tree_node_id,
    safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return safe_browsing::BrowserURLLoaderThrottle::Create(
      base::BindOnce(
          [](SafeBrowsingService* sb_service) {
            return sb_service->GetSafeBrowsingUrlCheckerDelegate();
          },
          base::Unretained(this)),
      wc_getter, frame_tree_node_id,
      url_lookup_service ? url_lookup_service->GetWeakPtr() : nullptr,
      /*hash_realtime_service=*/nullptr);
}

std::unique_ptr<content::NavigationThrottle>
SafeBrowsingService::MaybeCreateSafeBrowsingNavigationThrottleFor(
    content::NavigationHandle* handle) {
  if (!base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing)) {
    return nullptr;
  }

  return safe_browsing::SafeBrowsingNavigationThrottle::MaybeCreateThrottleFor(
      handle, GetSafeBrowsingUIManager().get());
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

scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
SafeBrowsingService::GetSafeBrowsingDBManager() {
  if (!safe_browsing_db_manager_) {
    CreateAndStartSafeBrowsingDBManager();
  }
  return safe_browsing_db_manager_;
}

scoped_refptr<safe_browsing::SafeBrowsingUIManager>
SafeBrowsingService::GetSafeBrowsingUIManager() {
  return ui_manager_;
}

safe_browsing::TriggerManager* SafeBrowsingService::GetTriggerManager() {
  return trigger_manager_.get();
}

void SafeBrowsingService::CreateSafeBrowsingUIManager() {
  DCHECK(!ui_manager_);
  ui_manager_ = new safe_browsing::SafeBrowsingUIManager(
      std::make_unique<WebLayerSafeBrowsingUIManagerDelegate>(),
      std::make_unique<WebLayerSafeBrowsingBlockingPageFactory>(),
      GURL(url::kAboutBlankURL));
}

void SafeBrowsingService::CreateTriggerManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  trigger_manager_ = std::make_unique<safe_browsing::TriggerManager>(
      ui_manager_.get(), BrowserProcess::GetInstance()->GetLocalState());
}

void SafeBrowsingService::CreateAndStartSafeBrowsingDBManager() {
  DCHECK(!safe_browsing_db_manager_);

  safe_browsing_db_manager_ =
      new safe_browsing::RemoteSafeBrowsingDatabaseManager();

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    // Posting a task to start the DB here ensures that it will be started by
    // the time that a consumer uses it on the IO thread, as such a consumer
    // would need to make it available for usage on the IO thread via a
    // PostTask() that will be ordered after this one.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SafeBrowsingService::StartSafeBrowsingDBManagerOnIOThread,
            base::Unretained(this)));
  } else {
    StartSafeBrowsingDBManagerOnIOThread();
  }
}

void SafeBrowsingService::StartSafeBrowsingDBManagerOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(safe_browsing_db_manager_);

  if (started_db_manager_) {
    return;
  }

  started_db_manager_ = true;

  // V4ProtocolConfig is not used. Just create one with empty values.
  safe_browsing::V4ProtocolConfig config("", false, "", "");
  safe_browsing_db_manager_->StartOnIOThread(GetURLLoaderFactoryOnIOThread(),
                                             config);
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingService::GetURLLoaderFactoryOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!shared_url_loader_factory_on_io_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingService::CreateURLLoaderFactoryForIO,
                       base::Unretained(this),
                       url_loader_factory_on_io_.BindNewPipeAndPassReceiver()));
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

void SafeBrowsingService::AddInterface(
    service_manager::BinderRegistry* registry,
    content::RenderProcessHost* render_process_host) {
  content::ResourceContext* resource_context =
      render_process_host->GetBrowserContext()->GetResourceContext();
  registry->AddInterface(
      base::BindRepeating(
          &MaybeCreateSafeBrowsing, render_process_host->GetID(),
          resource_context->GetWeakPtr(),
          base::BindRepeating(
              &SafeBrowsingService::GetSafeBrowsingUrlCheckerDelegate,
              base::Unretained(this))),
      content::GetUIThreadTaskRunner({}));
}

void SafeBrowsingService::StopDBManager() {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SafeBrowsingService::StopDBManagerOnIOThread,
                                base::Unretained(this)));
}

void SafeBrowsingService::StopDBManagerOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (safe_browsing_db_manager_) {
    safe_browsing_db_manager_->StopOnIOThread(true /*shutdown*/);
    safe_browsing_db_manager_.reset();
    started_db_manager_ = false;
  }
}

network::mojom::NetworkContext* SafeBrowsingService::GetNetworkContext() {
  if (!network_context_) {
    return nullptr;
  }
  return network_context_->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingService::GetURLLoaderFactory() {
  if (!network_context_) {
    return nullptr;
  }
  return network_context_->GetURLLoaderFactory();
}

}  // namespace weblayer
