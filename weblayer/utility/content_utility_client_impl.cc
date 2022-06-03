// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/utility/content_utility_client_impl.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"

namespace weblayer {

namespace {

base::LazyInstance<ContentUtilityClientImpl::NetworkBinderCreationCallback>::
    Leaky g_network_binder_creation_callback = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void ContentUtilityClientImpl::SetNetworkBinderCreationCallbackForTests(
    NetworkBinderCreationCallback callback) {
  g_network_binder_creation_callback.Get() = std::move(callback);
}

ContentUtilityClientImpl::ContentUtilityClientImpl() = default;

ContentUtilityClientImpl::~ContentUtilityClientImpl() = default;

void ContentUtilityClientImpl::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  if (g_network_binder_creation_callback.Get())
    g_network_binder_creation_callback.Get().Run(registry);
}

}  // namespace weblayer
