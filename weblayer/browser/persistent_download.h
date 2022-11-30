// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERSISTENT_DOWNLOAD_H_
#define WEBLAYER_BROWSER_PERSISTENT_DOWNLOAD_H_

#include "base/memory/raw_ptr.h"
#include "weblayer/browser/download_impl.h"

namespace download {
class DownloadItem;
}

namespace weblayer {

// A DownloadImpl that is persisted to disk. It will be backed by a
// download::DownloadItem for which IsTransient returns false. This is used when
// a user downloads a file from the web.
class PersistentDownload : public DownloadImpl {
 public:
  PersistentDownload(const PersistentDownload& other) = delete;
  PersistentDownload& operator=(const PersistentDownload& other) = delete;
  ~PersistentDownload() override;

  static void Create(download::DownloadItem* item);
  static PersistentDownload* Get(download::DownloadItem* item);

  // Download:
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
  explicit PersistentDownload(download::DownloadItem* item);

  void PauseInternal();
  void ResumeInternal();
  void CancelInternal();

  raw_ptr<download::DownloadItem> item_;

  bool pause_pending_ = false;
  bool resume_pending_ = false;
  bool cancel_pending_ = false;

  base::WeakPtrFactory<PersistentDownload> weak_ptr_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERSISTENT_DOWNLOAD_H_
