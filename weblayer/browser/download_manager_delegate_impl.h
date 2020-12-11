// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"

namespace weblayer {
class DownloadDelegate;

extern const char kDownloadNextIDPref[];

class DownloadManagerDelegateImpl : public content::DownloadManagerDelegate,
                                    public content::DownloadManager::Observer,
                                    public download::DownloadItem::Observer {
 public:
  explicit DownloadManagerDelegateImpl(
      content::DownloadManager* download_manager);
  ~DownloadManagerDelegateImpl() override;

  void set_download_dropped_closure_for_testing(
      const base::RepeatingClosure& callback) {
    download_dropped_callback_ = callback;
  }

 private:
  // content::DownloadManagerDelegate implementation:
  void GetNextId(content::DownloadIdCallback callback) override;
  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      content::DownloadTargetCallback* callback) override;
  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override;
  void GetSaveDir(content::BrowserContext* browser_context,
                  base::FilePath* website_save_dir,
                  base::FilePath* download_save_dir) override;
  void CheckDownloadAllowed(
      const content::WebContents::Getter& web_contents_getter,
      const GURL& url,
      const std::string& request_method,
      base::Optional<url::Origin> request_initiator,
      bool from_download_cross_origin_redirect,
      bool content_initiated,
      content::CheckDownloadAllowedCallback check_download_allowed_cb) override;

  // content::DownloadManager::Observer implementation:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadDropped(content::DownloadManager* manager) override;
  void OnManagerInitialized() override;

  // download::DownloadItem::Observer implementation:
  void OnDownloadUpdated(download::DownloadItem* item) override;

  void OnDownloadPathGenerated(uint32_t download_id,
                               content::DownloadTargetCallback callback,
                               const base::FilePath& suggested_path);
  void RemoveItem(const std::string& guid);

  // Helper methods to get a DownloadDelegate.
  DownloadDelegate* GetDelegate(content::WebContents* web_contents);
  DownloadDelegate* GetDelegate(content::BrowserContext* browser_context);
  DownloadDelegate* GetDelegate(download::DownloadItem* item);

  content::DownloadManager* download_manager_;
  base::RepeatingClosure download_dropped_callback_;
  base::WeakPtrFactory<DownloadManagerDelegateImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
