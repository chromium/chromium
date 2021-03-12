// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"
#include "weblayer/browser/background_fetch/background_fetch_download.h"

namespace content {
class BrowserContext;
}

namespace download {
class DownloadService;
}  // namespace download

namespace weblayer {

struct JobDetails;

// Implementation of BackgroundFetchDelegate using the DownloadService.
// TODO(estade): refactor and share this code with Chrome's version.
class BackgroundFetchDelegateImpl : public content::BackgroundFetchDelegate,
                                    public KeyedService {
 public:
  explicit BackgroundFetchDelegateImpl(content::BrowserContext* context);
  BackgroundFetchDelegateImpl(const BackgroundFetchDelegateImpl&) = delete;
  BackgroundFetchDelegateImpl& operator=(const BackgroundFetchDelegateImpl&) =
      delete;
  ~BackgroundFetchDelegateImpl() override;

  // Lazily initializes and returns the DownloadService.
  download::DownloadService* GetDownloadService();

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void GetPermissionForOrigin(const url::Origin& origin,
                              const content::WebContents::Getter& wc_getter,
                              GetPermissionForOriginCallback callback) override;
  void CreateDownloadJob(base::WeakPtr<Client> client,
                         std::unique_ptr<content::BackgroundFetchDescription>
                             fetch_description) override;
  void DownloadUrl(const std::string& job_id,
                   const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers,
                   bool has_request_body) override;
  void Abort(const std::string& job_id) override;
  void MarkJobComplete(const std::string& job_id) override;
  void UpdateUI(const std::string& job_id,
                const base::Optional<std::string>& title,
                const base::Optional<SkBitmap>& icon) override;

  // Abort all ongoing downloads and fail the fetch. Currently only used when
  // the bytes downloaded exceed the total download size, if specified.
  void FailFetch(const std::string& job_id);

  void OnDownloadStarted(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResponse> response);

  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded);

  void OnDownloadFailed(const std::string& guid,
                        std::unique_ptr<content::BackgroundFetchResult> result);

  void OnDownloadSucceeded(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResult> result);

  // Whether the provided GUID is resuming from the perspective of Background
  // Fetch.
  bool IsGuidOutstanding(const std::string& guid) const;

  // Notifies the OfflineContentAggregator of an interrupted download that is
  // in a paused state.
  void RestartPausedDownload(const std::string& download_guid);

  // Returns the set of download GUIDs that have started but did not finish
  // according to Background Fetch. Clears out all references to outstanding
  // GUIDs.
  std::set<std::string> TakeOutstandingGuids();

  // Gets the upload data, if any, associated with the |download_guid|.
  void GetUploadData(const std::string& download_guid,
                     download::GetUploadDataCallback callback);

  base::WeakPtr<BackgroundFetchDelegateImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Called by BackgroundFetchDownload. The implementation should be shared with
  // Chrome.
  void PauseDownload(const std::string& job_id);
  void ResumeDownload(const std::string& job_id);
  void CancelDownload(const std::string& job_id);

  // TODO(estade): make these override virtual methods from a base class that's
  // shared with Chrome.
  void OnJobCreated(const std::string& job_id, const JobDetails* job);
  void UpdateUiAndUpdateObservers(const std::string& job_id);

 private:
  // Starts a download according to |params| belonging to |job_id|.
  void StartDownload(const std::string& job_id,
                     const download::DownloadParams& params,
                     bool has_request_body);

  void OnDownloadReceived(const std::string& guid,
                          download::DownloadParams::StartResult result);

  // The callback passed to DownloadRequestLimiter::CanDownload().
  void DidGetPermissionFromDownloadRequestLimiter(
      GetPermissionForOriginCallback callback,
      bool has_permission);

  void DidGetUploadData(const std::string& unique_id,
                        const std::string& download_guid,
                        download::GetUploadDataCallback callback,
                        blink::mojom::SerializedBlobPtr blob);

  // Returns the client for a given |job_id|.
  base::WeakPtr<Client> GetClient(const std::string& job_id);

  // The browser context this service is being created for.
  content::BrowserContext* context_;

  // Map from individual download GUIDs to job unique ids.
  std::map<std::string, std::string> download_job_id_map_;

  // Map from job unique ids to the details of the job.
  // TODO(estade): this map needs to have its items erased at some point.
  std::map<std::string, JobDetails> job_details_map_;

  // Map from job unique ids to the UI item for the job.
  // TODO(estade): this map needs to have its items erased at some point.
  std::map<std::string, BackgroundFetchDownload> ui_item_map_;

  std::unique_ptr<download::DownloadService> download_service_;

  base::WeakPtrFactory<BackgroundFetchDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_IMPL_H_
