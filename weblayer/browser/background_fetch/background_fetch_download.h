// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DOWNLOAD_H_
#define WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DOWNLOAD_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "weblayer/browser/download_impl.h"

namespace background_fetch {
struct JobDetails;
}

namespace weblayer {

class BackgroundFetchDelegateImpl;

// The UI object for an in-progress BackgroundFetch download job.
class BackgroundFetchDownload : public DownloadImpl {
 public:
  BackgroundFetchDownload(BackgroundFetchDelegateImpl* controller,
                          const std::string& job_id,
                          const background_fetch::JobDetails* job);
  BackgroundFetchDownload(const BackgroundFetchDownload& other) = delete;
  BackgroundFetchDownload& operator=(const BackgroundFetchDownload& other) =
      delete;
  ~BackgroundFetchDownload() override;

  // Download implementation:
  DownloadState GetState() override;
  int64_t GetTotalBytes() override;
  int64_t GetReceivedBytes() override;
  void Pause() override;
  void Resume() override;
  void Cancel() override;
  base::FilePath GetLocation() override;
  std::u16string GetFileNameToReportToUser() override;
  std::string GetMimeType() override;
  DownloadError GetError() override;

  // DownloadImpl:
  int GetNotificationId() override;
  bool IsTransient() override;
  GURL GetSourceUrl() override;
  const SkBitmap* GetLargeIcon() override;
  void OnFinished(bool activated) override;

 private:
  raw_ptr<BackgroundFetchDelegateImpl> controller_;
  std::string job_id_;
  int notification_id_ = 0;
  raw_ptr<const background_fetch::JobDetails> job_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DOWNLOAD_H_
