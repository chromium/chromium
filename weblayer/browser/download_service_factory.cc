// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_service_factory.h"

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/background_fetch/download_client.h"
#include "components/download/content/factory/download_service_factory_helper.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/public/background_service/basic_task_scheduler.h"
#include "components/download/public/background_service/blob_context_getter_factory.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/system_network_context_manager.h"

namespace weblayer {

namespace {

// Like DownloadServiceFactory, this is a BrowserContextKeyedServiceFactory
// although the Chrome version is a SimpleKeyedServiceFactory.
class SimpleDownloadManagerCoordinatorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of SimpleDownloadManagerCoordinatorFactory.
  static SimpleDownloadManagerCoordinatorFactory* GetInstance() {
    static base::NoDestructor<SimpleDownloadManagerCoordinatorFactory> instance;
    return instance.get();
  }

  static download::SimpleDownloadManagerCoordinator* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<download::SimpleDownloadManagerCoordinator*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  SimpleDownloadManagerCoordinatorFactory(
      const SimpleDownloadManagerCoordinatorFactory& other) = delete;
  SimpleDownloadManagerCoordinatorFactory& operator=(
      const SimpleDownloadManagerCoordinatorFactory& other) = delete;

 private:
  friend class base::NoDestructor<SimpleDownloadManagerCoordinatorFactory>;

  SimpleDownloadManagerCoordinatorFactory()
      : BrowserContextKeyedServiceFactory(
            "SimpleDownloadManagerCoordinator",
            BrowserContextDependencyManager::GetInstance()) {}
  ~SimpleDownloadManagerCoordinatorFactory() override = default;

  // BrowserContextKeyedService:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    auto* service_instance = new download::SimpleDownloadManagerCoordinator(
        {}, !context->IsOffTheRecord());
    service_instance->SetSimpleDownloadManager(
        content::BrowserContext::GetDownloadManager(context), true);
    return service_instance;
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
};

// An implementation of BlobContextGetterFactory that returns the
// BlobStorageContext without delay (since WebLayer must be in "full browser"
// mode).
class DownloadBlobContextGetterFactory
    : public download::BlobContextGetterFactory {
 public:
  explicit DownloadBlobContextGetterFactory(
      content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  DownloadBlobContextGetterFactory(const DownloadBlobContextGetterFactory&) =
      delete;
  DownloadBlobContextGetterFactory& operator=(
      const DownloadBlobContextGetterFactory&) = delete;
  ~DownloadBlobContextGetterFactory() override = default;

 private:
  // download::BlobContextGetterFactory:
  void RetrieveBlobContextGetter(
      download::BlobContextGetterCallback callback) override {
    std::move(callback).Run(
        content::BrowserContext::GetBlobStorageContext(browser_context_));
  }

  content::BrowserContext* browser_context_;
};

}  // namespace

// static
download::DownloadService* DownloadServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<download::DownloadService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
DownloadServiceFactory* DownloadServiceFactory::GetInstance() {
  static base::NoDestructor<DownloadServiceFactory> factory;
  return factory.get();
}

DownloadServiceFactory::DownloadServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DownloadServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SimpleDownloadManagerCoordinatorFactory::GetInstance());
  DependsOn(download::NavigationMonitorFactory::GetInstance());
}

KeyedService* DownloadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  SimpleFactoryKey* key = ProfileImpl::FromBrowserContext(context)
                              ->GetBrowserContext()
                              ->simple_factory_key();

  auto clients = std::make_unique<download::DownloadClientMap>();
  clients->insert(std::make_pair(
      download::DownloadClient::BACKGROUND_FETCH,
      std::make_unique<background_fetch::DownloadClient>(context)));

  // Build in memory download service for an off the record context.
  if (context->IsOffTheRecord()) {
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
        content::GetIOThreadTaskRunner({});
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();

    return download::BuildInMemoryDownloadService(
               key, std::move(clients), content::GetNetworkConnectionTracker(),
               base::FilePath(),
               std::make_unique<DownloadBlobContextGetterFactory>(context),
               io_task_runner, url_loader_factory)
        .release();
  }

  // Build download service for a regular browsing context.
  base::FilePath storage_dir;
  if (!context->IsOffTheRecord() && !context->GetPath().empty()) {
    const base::FilePath::CharType kDownloadServiceStorageDirname[] =
        FILE_PATH_LITERAL("Download Service");
    storage_dir = context->GetPath().Append(kDownloadServiceStorageDirname);
  }
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  leveldb_proto::ProtoDatabaseProvider* proto_db_provider =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetProtoDatabaseProvider();
  return download::BuildDownloadService(
             key, std::move(clients), content::GetNetworkConnectionTracker(),
             storage_dir,
             SimpleDownloadManagerCoordinatorFactory::GetForBrowserContext(
                 context),
             proto_db_provider, background_task_runner,
             std::make_unique<download::BasicTaskScheduler>(base::BindRepeating(
                 [](content::BrowserContext* context) {
                   return DownloadServiceFactory::GetForBrowserContext(context);
                 },
                 context)))
      .release();
}

content::BrowserContext* DownloadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
