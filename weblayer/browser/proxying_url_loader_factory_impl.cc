// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/proxying_url_loader_factory_impl.h"

#include "base/time/time.h"
#include "components/embedder_support/android/util/android_stream_reader_url_loader.h"
#include "components/embedder_support/android/util/response_delegate_impl.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"

namespace weblayer {

namespace {

const char kCacheKey[] = "weblayer_entry_cache_data";

struct WriteData {
  mojo::Remote<network::mojom::URLLoaderClient> client;
  std::string data;
  std::unique_ptr<mojo::DataPipeProducer> producer;
};

void OnWrite(std::unique_ptr<WriteData> write_data, MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    write_data->client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = write_data->data.size();
  status.encoded_body_length = write_data->data.size();
  status.decoded_body_length = write_data->data.size();
  write_data->client->OnComplete(status);
}

void StartCachedLoad(
    mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client,
    network::mojom::URLResponseHeadPtr response_head,
    const std::string& data) {
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(pending_client));
  client->OnReceiveResponse(std::move(response_head));

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client->OnStartLoadingResponseBody(std::move(consumer));

  auto write_data = std::make_unique<WriteData>();
  write_data->client = std::move(client);
  write_data->data = std::move(data);
  write_data->producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer));

  mojo::DataPipeProducer* producer_ptr = write_data->producer.get();
  base::StringPiece string_piece(write_data->data);

  producer_ptr->Write(
      std::make_unique<mojo::StringDataSource>(
          string_piece, mojo::StringDataSource::AsyncWritingMode::
                            STRING_STAYS_VALID_UNTIL_COMPLETION),
      base::BindOnce(OnWrite, std::move(write_data)));
}

// Returns a NavigationEntry (pending or committed) for the given id if it
// exists.
content::NavigationEntry* GetNavigationEntryFromUniqueId(
    int frame_tree_node_id,
    int navigation_entry_unique_id) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return nullptr;
  auto& controller = web_contents->GetController();
  auto* pending_entry = controller.GetPendingEntry();
  if (pending_entry &&
      pending_entry->GetUniqueID() == navigation_entry_unique_id) {
    return pending_entry;
  }

  // Entry might have committed.
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    if (controller.GetEntryAtIndex(i)->GetUniqueID() ==
        navigation_entry_unique_id)
      return controller.GetEntryAtIndex(i);
  }

  return nullptr;
}

// Returns true if the response headers indicate that the response is still
// valid without going over the network.
bool IsCachedResponseValid(net::HttpResponseHeaders* headers,
                           base::Time request_time,
                           base::Time response_time) {
  return headers->RequiresValidation(request_time, response_time,
                                     base::Time::Now()) == net::VALIDATION_NONE;
}

// Stored on the NavigationEntry when we have a cached response from an
// InputStream.
class NavigationEntryCache : public base::SupportsUserData::Data {
 public:
  NavigationEntryCache(network::mojom::URLResponseHeadPtr response_head,
                       const std::string& data,
                       base::Time request_time,
                       base::Time response_time)
      : response_head_(std::move(response_head)),
        data_(data),
        request_time_(request_time),
        response_time_(response_time) {}

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<NavigationEntryCache>(
        response_head_.Clone(), data_, request_time_, response_time_);
  }

  network::mojom::URLResponseHead* response_head() {
    return response_head_.get();
  }
  const std::string& data() const { return data_; }
  const base::Time& request_time() const { return request_time_; }
  const base::Time& response_time() const { return response_time_; }

 private:
  network::mojom::URLResponseHeadPtr response_head_;
  std::string data_;
  base::Time request_time_;
  base::Time response_time_;
};

// A ResponseDelegate for AndroidStreamReaderURLLoader which will cache the
// response if it's successful. This allows back-forward navigations to reuse an
// InputStream.
class CachingResponseDelegate : public embedder_support::ResponseDelegateImpl {
 public:
  CachingResponseDelegate(
      std::unique_ptr<embedder_support::WebResourceResponse> response,
      int frame_tree_node_id,
      int navigation_entry_unique_id)
      : ResponseDelegateImpl(std::move(response)),
        frame_tree_node_id_(frame_tree_node_id),
        navigation_entry_unique_id_(navigation_entry_unique_id),
        request_time_(base::Time::Now()) {}

  ~CachingResponseDelegate() override = default;

  // embedder_support::ResponseDelegateImpl implementation:
  bool ShouldCacheResponse(network::mojom::URLResponseHead* response) override {
    response_time_ = base::Time::Now();

    // If at this point the response isn't cacheable it'll never be.
    if (!IsCachedResponseValid(response->headers.get(), request_time_,
                               response_time_)) {
      return false;
    }

    response_head_ = response->Clone();
    return true;
  }

  void OnResponseCache(const std::string& data) override {
    content::NavigationEntry* entry = GetNavigationEntryFromUniqueId(
        frame_tree_node_id_, navigation_entry_unique_id_);
    if (!entry)
      return;

    auto cache_data = std::make_unique<NavigationEntryCache>(
        std::move(response_head_), data, request_time_, response_time_);
    entry->SetUserData(kCacheKey, std::move(cache_data));
  }

 private:
  int frame_tree_node_id_;
  int navigation_entry_unique_id_;

  // The time that this object was created.
  base::Time request_time_;
  // The time that we sent the response headers.
  base::Time response_time_;

  network::mojom::URLResponseHeadPtr response_head_;
};

}  // namespace

ProxyingURLLoaderFactoryImpl::ProxyingURLLoaderFactoryImpl(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    const GURL& url_for_response,
    std::unique_ptr<embedder_support::WebResourceResponse> response,
    int frame_tree_node_id,
    int navigation_entry_unique_id)
    : url_for_response_(url_for_response),
      response_(std::move(response)),
      frame_tree_node_id_(frame_tree_node_id),
      navigation_entry_unique_id_(navigation_entry_unique_id) {
  DCHECK(response_ ||
         HasCachedInputStream(frame_tree_node_id, navigation_entry_unique_id));
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&ProxyingURLLoaderFactoryImpl::OnTargetFactoryError,
                     base::Unretained(this)));

  proxy_receivers_.Add(this, std::move(loader_receiver));
  proxy_receivers_.set_disconnect_handler(
      base::BindRepeating(&ProxyingURLLoaderFactoryImpl::OnProxyBindingError,
                          base::Unretained(this)));
}

ProxyingURLLoaderFactoryImpl::~ProxyingURLLoaderFactoryImpl() = default;

bool ProxyingURLLoaderFactoryImpl::HasCachedInputStream(
    int frame_tree_node_id,
    int navigation_entry_unique_id) {
  auto* entry = GetNavigationEntryFromUniqueId(frame_tree_node_id,
                                               navigation_entry_unique_id);
  if (!entry)
    return false;

  auto* cache =
      static_cast<NavigationEntryCache*>(entry->GetUserData(kCacheKey));
  if (!cache)
    return false;

  if (!IsCachedResponseValid(cache->response_head()->headers.get(),
                             cache->request_time(), cache->response_time())) {
    // Cache expired so remove it.
    entry->RemoveUserData(kCacheKey);
    return false;
  }

  return true;
}

void ProxyingURLLoaderFactoryImpl::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (url_for_response_ == request.url) {
    if (response_) {
      auto* stream_loader = new embedder_support::AndroidStreamReaderURLLoader(
          request, std::move(client), traffic_annotation,
          std::make_unique<CachingResponseDelegate>(
              std::move(response_), frame_tree_node_id_,
              navigation_entry_unique_id_),
          base::nullopt);
      stream_loader->Start();
      return;
    }

    if (HasCachedInputStream(frame_tree_node_id_,
                             navigation_entry_unique_id_)) {
      auto* entry = GetNavigationEntryFromUniqueId(frame_tree_node_id_,
                                                   navigation_entry_unique_id_);
      auto* cache =
          static_cast<NavigationEntryCache*>(entry->GetUserData(kCacheKey));
      StartCachedLoad(std::move(client), cache->response_head()->Clone(),
                      cache->data());
      return;
    }
  }

  target_factory_->CreateLoaderAndStart(std::move(loader), routing_id,
                                        request_id, options, request,
                                        std::move(client), traffic_annotation);
}

void ProxyingURLLoaderFactoryImpl::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void ProxyingURLLoaderFactoryImpl::OnTargetFactoryError() {
  delete this;
}

void ProxyingURLLoaderFactoryImpl::OnProxyBindingError() {
  if (proxy_receivers_.empty())
    delete this;
}

}  // namespace weblayer
