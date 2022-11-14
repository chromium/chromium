// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/persistent_download.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/download/public/common/download_item.h"

namespace weblayer {

namespace {
const char kPersistentDownloadKeyName[] = "weblayer_download_impl";
}

PersistentDownload::~PersistentDownload() = default;

void PersistentDownload::Create(download::DownloadItem* item) {
  item->SetUserData(kPersistentDownloadKeyName,
                    base::WrapUnique(new PersistentDownload(item)));
}

PersistentDownload* PersistentDownload::Get(download::DownloadItem* item) {
  return static_cast<PersistentDownload*>(
      item->GetUserData(kPersistentDownloadKeyName));
}

DownloadState PersistentDownload::GetState() {
  if (item_->GetState() == download::DownloadItem::COMPLETE)
    return DownloadState::kComplete;

  if (cancel_pending_ || item_->GetState() == download::DownloadItem::CANCELLED)
    return DownloadState::kCancelled;

  if (pause_pending_ || (item_->IsPaused() && !resume_pending_))
    return DownloadState::kPaused;

  if (resume_pending_ ||
      item_->GetState() == download::DownloadItem::IN_PROGRESS) {
    return DownloadState::kInProgress;
  }

  return DownloadState::kFailed;
}

int64_t PersistentDownload::GetTotalBytes() {
  return item_->GetTotalBytes();
}

int64_t PersistentDownload::GetReceivedBytes() {
  return item_->GetReceivedBytes();
}

void PersistentDownload::Pause() {
  // The Pause/Resume/Cancel methods need to be called in a PostTask because we
  // may be in a callback from the download subsystem and it doesn't handle
  // nested calls.
  resume_pending_ = false;
  pause_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PersistentDownload::PauseInternal,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PersistentDownload::Resume() {
  pause_pending_ = false;
  resume_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PersistentDownload::ResumeInternal,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PersistentDownload::Cancel() {
  cancel_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PersistentDownload::CancelInternal,
                                weak_ptr_factory_.GetWeakPtr()));
}

base::FilePath PersistentDownload::GetLocation() {
  return item_->GetTargetFilePath();
}

std::u16string PersistentDownload::GetFileNameToReportToUser() {
  return item_->GetFileNameToReportUser().LossyDisplayName();
}

std::string PersistentDownload::GetMimeType() {
  return item_->GetMimeType();
}

DownloadError PersistentDownload::GetError() {
  auto reason = item_->GetLastReason();
  if (reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
    return DownloadError::kNoError;

  if (reason == download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM)
    return DownloadError::kSSLError;

  if (reason >= download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED &&
      reason <=
          download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT)
    return DownloadError::kServerError;

  if (reason >= download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED &&
      reason <= download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST)
    return DownloadError::kConnectivityError;

  if (reason == download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE)
    return DownloadError::kNoSpace;

  if (reason >= download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED &&
      reason <= download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE)
    return DownloadError::kFileError;

  if (reason == download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED)
    return DownloadError::kCancelled;

  return DownloadError::kOtherError;
}

PersistentDownload::PersistentDownload(download::DownloadItem* item)
    : item_(item) {}

void PersistentDownload::PauseInternal() {
  if (pause_pending_) {
    pause_pending_ = false;
    item_->Pause();
  }
}

int PersistentDownload::GetNotificationId() {
  return item_->GetId();
}

bool PersistentDownload::IsTransient() {
  return false;
}

GURL PersistentDownload::GetSourceUrl() {
  return {};
}

const SkBitmap* PersistentDownload::GetLargeIcon() {
  return nullptr;
}

void PersistentDownload::OnFinished(bool activated) {
  // For this type of download, activation is handled purely in Java.
}

void PersistentDownload::ResumeInternal() {
  if (resume_pending_) {
    resume_pending_ = false;
    item_->Resume(true);
  }
}

void PersistentDownload::CancelInternal() {
  cancel_pending_ = false;
  item_->Cancel(true);
}

}  // namespace weblayer
