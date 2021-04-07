// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SUBRESOURCE_FILTER_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_SUBRESOURCE_FILTER_CLIENT_IMPL_H_

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace subresource_filter {
class ContentSubresourceFilterThrottleManager;
}  // namespace subresource_filter

namespace weblayer {

// WebLayer implementation of SubresourceFilterClient. Instances are associated
// with and owned by ContentSubresourceFilterThrottleManager instances.
class SubresourceFilterClientImpl
    : public subresource_filter::SubresourceFilterClient {
 public:
  explicit SubresourceFilterClientImpl(content::WebContents* web_contents);
  ~SubresourceFilterClientImpl() override;

  SubresourceFilterClientImpl(const SubresourceFilterClientImpl&) = delete;
  SubresourceFilterClientImpl& operator=(const SubresourceFilterClientImpl&) =
      delete;

  // Creates a ContentSubresourceFilterThrottleManager and attaches it to
  // |web_contents|, passing it an instance of this client and other
  // embedder-level state.
  static void CreateThrottleManagerWithClientForWebContents(
      content::WebContents* web_contents);

  // SubresourceFilterClient:
  void ShowNotification() override;
  const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetSafeBrowsingDatabaseManager() override;

  // Sets the SafeBrowsingDatabaseManager instance used to |database_manager|.
  void set_database_manager_for_testing(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager) {
    database_manager_ = std::move(database_manager);
  }

 private:
  // This member is only used on Android, so it's necessary to ifdef it to avoid
  // a compiler error on other platforms.
#if defined(OS_ANDROID)
  content::WebContents* web_contents_;
#endif
  std::unique_ptr<subresource_filter::ContentSubresourceFilterThrottleManager>
      throttle_manager_;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SUBRESOURCE_FILTER_CLIENT_IMPL_H_
