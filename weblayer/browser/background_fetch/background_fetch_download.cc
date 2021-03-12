// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_fetch/background_fetch_download.h"

#include "base/files/file_path.h"
#include "content/public/browser/background_fetch_description.h"
#include "weblayer/browser/background_fetch/background_fetch_delegate_impl.h"
#include "weblayer/browser/background_fetch/job_details.h"

namespace weblayer {

BackgroundFetchDownload::BackgroundFetchDownload(
    BackgroundFetchDelegateImpl* controller,
    const std::string& job_id,
    const JobDetails* job)
    : controller_(controller), job_id_(job_id), job_(job) {
  // The only other kind of DownloadImpl uses the DownloadItem's ID as its
  // notification ID, and that's constrained to positive integers.
  static int id = 0;
  if (id > 0)
    id = 0;
  notification_id_ = --id;
}

BackgroundFetchDownload::~BackgroundFetchDownload() = default;

DownloadState BackgroundFetchDownload::GetState() {
  switch (job_->job_state) {
    case JobDetails::State::kPendingWillStartDownloading:
    case JobDetails::State::kStartedAndDownloading:
      return DownloadState::kInProgress;

    case JobDetails::State::kPendingWillStartPaused:
    case JobDetails::State::kStartedButPaused:
      return DownloadState::kPaused;

    case JobDetails::State::kCancelled:
      return DownloadState::kCancelled;

    case JobDetails::State::kDownloadsComplete:
    case JobDetails::State::kJobComplete:
      return DownloadState::kComplete;
  }
}

int64_t BackgroundFetchDownload::GetTotalBytes() {
  return job_->GetTotalBytes();
}

int64_t BackgroundFetchDownload::GetReceivedBytes() {
  return job_->GetProcessedBytes();
}

void BackgroundFetchDownload::Pause() {
  controller_->PauseDownload(job_id_);
}

void BackgroundFetchDownload::Resume() {
  controller_->ResumeDownload(job_id_);
}

void BackgroundFetchDownload::Cancel() {
  controller_->CancelDownload(job_id_);
}

base::FilePath BackgroundFetchDownload::GetLocation() {
  NOTREACHED();
  return {};
}

base::FilePath BackgroundFetchDownload::GetFileNameToReportToUser() {
  // TODO(estade): this method should return a string instead of a FilePath.
  // It's used as the notification's title.
  return base::FilePath::FromUTF8Unsafe(job_->fetch_description->title);
}

std::string BackgroundFetchDownload::GetMimeType() {
  NOTREACHED();
  return {};
}

DownloadError BackgroundFetchDownload::GetError() {
  NOTREACHED();
  return DownloadError::kNoError;
}

int BackgroundFetchDownload::GetNotificationId() {
  return notification_id_;
}

bool BackgroundFetchDownload::IsTransient() {
  return true;
}

}  // namespace weblayer
