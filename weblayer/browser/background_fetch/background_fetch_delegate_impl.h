// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/background_fetch/background_fetch_delegate_base.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"
#include "weblayer/browser/background_fetch/background_fetch_download.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

// Implementation of BackgroundFetchDelegate using the
// BackgroundDownloadService.
class BackgroundFetchDelegateImpl
    : public background_fetch::BackgroundFetchDelegateBase,
      public KeyedService {
 public:
  explicit BackgroundFetchDelegateImpl(content::BrowserContext* context);
  BackgroundFetchDelegateImpl(const BackgroundFetchDelegateImpl&) = delete;
  BackgroundFetchDelegateImpl& operator=(const BackgroundFetchDelegateImpl&) =
      delete;
  ~BackgroundFetchDelegateImpl() override;

  // BackgroundFetchDelegate:
  void MarkJobComplete(const std::string& job_id) override;
  void UpdateUI(const std::string& job_id,
                const absl::optional<std::string>& title,
                const absl::optional<SkBitmap>& icon) override;

 protected:
  // BackgroundFetchDelegateBase:
  download::BackgroundDownloadService* GetDownloadService() override;
  void OnJobDetailsCreated(const std::string& job_id) override;
  void DoShowUi(const std::string& job_id) override;
  void DoUpdateUi(const std::string& job_id) override;
  void DoCleanUpUi(const std::string& job_id) override;

 private:
  // Map from job unique ids to the UI item for the job.
  std::map<std::string, BackgroundFetchDownload> ui_item_map_;

  base::WeakPtrFactory<BackgroundFetchDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
