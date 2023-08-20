// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_BROWSER_DOWNLOADS_WOLVIC_DOWNLOAD_MANAGER_DELEGATE_H_
#define WOLVIC_WOLVIC_BROWSER_DOWNLOADS_WOLVIC_DOWNLOAD_MANAGER_DELEGATE_H_

#include "content/public/browser/download_manager_delegate.h"

namespace wolvic {

class WolvicDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  WolvicDownloadManagerDelegate() = default;

  WolvicDownloadManagerDelegate(const WolvicDownloadManagerDelegate&) = delete;
  WolvicDownloadManagerDelegate& operator=(
      const WolvicDownloadManagerDelegate&) = delete;

  ~WolvicDownloadManagerDelegate() override = default;

  // content::DownloadManagerDelegate implementation.
  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override;
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_BROWSER_DOWNLOADS_WOLVIC_DOWNLOAD_MANAGER_DELEGATE_H_
