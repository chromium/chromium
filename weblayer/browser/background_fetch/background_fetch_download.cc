// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_fetch/background_fetch_download.h"

#include "base/strings/utf_string_conversions.h"
#include "components/background_fetch/job_details.h"
#include "content/public/browser/background_fetch_description.h"
#include "url/origin.h"
#include "weblayer/browser/background_fetch/background_fetch_delegate_impl.h"

using background_fetch::JobDetails;

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

std::u16string BackgroundFetchDownload::GetFileNameToReportToUser() {
  return base::UTF8ToUTF16(job_->fetch_description->title);
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

GURL BackgroundFetchDownload::GetSourceUrl() {
  return job_->fetch_description->origin.GetURL();
}

const SkBitmap* BackgroundFetchDownload::GetLargeIcon() {
  return &job_->fetch_description->icon;
}

void BackgroundFetchDownload::OnFinished(bool activated) {
  if (activated)
    controller_->OnUiActivated(job_id_);
  controller_->OnUiFinished(job_id_);
  // |this| is deleted.
}

}  // namespace weblayer
