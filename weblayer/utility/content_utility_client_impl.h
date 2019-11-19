// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_UTILITY_CONTENT_UTILITY_CLIENT_IMPL_H_
#define WEBLAYER_UTILITY_CONTENT_UTILITY_CLIENT_IMPL_H_

#include <string>

#include "base/callback.h"
#include "content/public/utility/content_utility_client.h"

namespace weblayer {

class ContentUtilityClientImpl : public content::ContentUtilityClient {
 public:
  using NetworkBinderCreationCallback =
      base::RepeatingCallback<void(service_manager::BinderRegistry*)>;

  static void SetNetworkBinderCreationCallbackForTests(
      NetworkBinderCreationCallback callback);

  ContentUtilityClientImpl();
  ~ContentUtilityClientImpl() override;

  // content::ContentUtilityClient:
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentUtilityClientImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_UTILITY_CONTENT_UTILITY_CLIENT_IMPL_H_
