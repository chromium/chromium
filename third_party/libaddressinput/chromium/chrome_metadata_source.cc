// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace autofill {

ChromeMetadataSource::ChromeMetadataSource(
    const std::string& validation_data_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : validation_data_url_(validation_data_url),
      url_loader_factory_(std::move(url_loader_factory)) {}

ChromeMetadataSource::~ChromeMetadataSource() {}

void ChromeMetadataSource::Get(const std::string& key,
                               const Callback& downloaded) const {
  const_cast<ChromeMetadataSource*>(this)->Download(key, downloaded);
}

void ChromeMetadataSource::OnSimpleLoaderComplete(
    RequestList::iterator it,
    std::unique_ptr<std::string> response_body) {
  const Callback& callback = it->get()->callback;
  const std::string& key = it->get()->key;
  std::unique_ptr<std::string> data(new std::string());
  bool ok = !!response_body;
  if (ok)
    data->swap(*response_body);
  callback(ok, key, data.release());
  requests_.erase(it);
}

ChromeMetadataSource::Request::Request(
    const std::string& key,
    std::unique_ptr<network::SimpleURLLoader> loader,
    const Callback& callback)
    : key(key), loader(std::move(loader)), callback(callback) {}

void ChromeMetadataSource::Download(const std::string& key,
                                    const Callback& downloaded) {
  GURL resource(validation_data_url_ + key);
  if (!resource.SchemeIsCryptographic()) {
    downloaded(false, key, NULL);
    return;
  }
  DCHECK(url_loader_factory_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("lib_address_input", R"(
        semantics {
          sender: "Address Format Metadata"
          description:
            "Address format metadata assists in handling postal addresses from "
            "all over the world."
          trigger:
            "User edits an address in Chromium settings, or shipping address "
            "in Android's 'web payments'."
          data:
            "The country code for the address being edited. No user identifier "
            "is sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. It can only be "
            "prevented if user does not edit the address in Android 'Web "
            "Payments' settings, Android's Chromium settings ('Autofill and "
            "payments' -> 'Addresses'), and Chromium settings on desktop ("
            "'Manage Autofill Settings' -> 'Addresses')."
          policy_exception_justification: "Not implemented."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = resource;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  auto it = requests_.insert(
      requests_.begin(),
      std::make_unique<Request>(key, std::move(loader), downloaded));
  network::SimpleURLLoader* raw_loader = it->get()->loader.get();
  raw_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ChromeMetadataSource::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(it)));
}

}  // namespace autofill
