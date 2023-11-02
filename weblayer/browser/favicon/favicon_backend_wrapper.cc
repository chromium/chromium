// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_backend_wrapper.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/favicon/core/favicon_backend.h"
#include "components/favicon/core/favicon_database.h"

namespace weblayer {

// Removing out of date entries can be costly. To avoid blocking the thread
// this code runs on, the work is potentially throttled. Specifically at
// most |kMaxNumberOfEntriesToRemoveAtATime| are removed during a single call.
// If |kMaxNumberOfEntriesToRemoveAtATime| are removed, then there may be more
// entries that can be removed, so the timer is restarted with a shorter time
// out (|kTimeDeltaForRunningExpireWithRemainingWork|).
constexpr base::TimeDelta kTimeDeltaForRunningExpireNoRemainingWork =
    base::Hours(1);
constexpr int kMaxNumberOfEntriesToRemoveAtATime = 100;

FaviconBackendWrapper::FaviconBackendWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : base::RefCountedDeleteOnSequence<FaviconBackendWrapper>(task_runner),
      task_runner_(task_runner) {}

void FaviconBackendWrapper::Init(const base::FilePath& db_path) {
  db_path_ = db_path;
  favicon_backend_ = favicon::FaviconBackend::Create(db_path, this);
  if (!favicon_backend_) {
    LOG(WARNING) << "Could not initialize the favicon database.";

    // The favicon db is not critical. On failure initializing try deleting
    // the file and repeating. Note that FaviconDatabase already tries to
    // initialize twice.
    base::DeleteFile(db_path);

    favicon_backend_ = favicon::FaviconBackend::Create(db_path, this);
    if (!favicon_backend_) {
      LOG(WARNING) << "Could not initialize db second time, giving up.";
      return;
    }
  }

  expire_timer_.Start(FROM_HERE, kTimeDeltaForRunningExpireWithRemainingWork,
                      this, &FaviconBackendWrapper::OnExpireTimerFired);
}

void FaviconBackendWrapper::Shutdown() {
  // Ensures there isn't a reference to this in the task runner (by way of the
  // task the timer posts).
  commit_timer_.Stop();
  expire_timer_.Stop();
}

void FaviconBackendWrapper::DeleteAndRecreateDatabase() {
  Shutdown();
  favicon_backend_.reset();
  base::DeleteFile(db_path_);
  Init(db_path_);
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackendWrapper::GetFaviconsForUrl(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    const std::vector<int>& desired_sizes) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->GetFaviconsForUrl(page_url, icon_types,
                                             desired_sizes,
                                             /* fallback_to_host */ false);
}

favicon_base::FaviconRawBitmapResult
FaviconBackendWrapper::GetLargestFaviconForUrl(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types_list,
    int minimum_size_in_pixels) {
  if (!favicon_backend_)
    return {};
  return favicon_backend_->GetLargestFaviconForUrl(page_url, icon_types_list,
                                                   minimum_size_in_pixels);
}

void FaviconBackendWrapper::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  if (favicon_backend_ &&
      favicon_backend_->SetFaviconsOutOfDateForPage(page_url)) {
    ScheduleCommit();
  }
}

void FaviconBackendWrapper::SetFavicons(const base::flat_set<GURL>& page_urls,
                                        favicon_base::IconType icon_type,
                                        const GURL& icon_url,
                                        const std::vector<SkBitmap>& bitmaps) {
  if (favicon_backend_ &&
      favicon_backend_
          ->SetFavicons(page_urls, icon_type, icon_url, bitmaps,
                        favicon::FaviconBitmapType::ON_VISIT)
          .did_change_database()) {
    ScheduleCommit();
  }
}

void FaviconBackendWrapper::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  if (!favicon_backend_)
    return;

  std::set<GURL> changed_urls = favicon_backend_->CloneFaviconMappingsForPages(
      {page_url_to_read}, icon_types, page_urls_to_write);
  if (!changed_urls.empty())
    ScheduleCommit();
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackendWrapper::GetFavicon(const GURL& icon_url,
                                  favicon_base::IconType icon_type,
                                  const std::vector<int>& desired_sizes) {
  return UpdateFaviconMappingsAndFetch({}, icon_url, icon_type, desired_sizes);
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackendWrapper::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  if (!favicon_backend_)
    return {};
  auto result = favicon_backend_->UpdateFaviconMappingsAndFetch(
      page_urls, icon_url, icon_type, desired_sizes);
  if (!result.updated_page_urls.empty())
    ScheduleCommit();
  return result.bitmap_results;
}

void FaviconBackendWrapper::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  if (!favicon_backend_)
    return;

  auto deleted_page_urls =
      favicon_backend_->DeleteFaviconMappings(page_urls, icon_type);
  if (!deleted_page_urls.empty())
    ScheduleCommit();
}

std::vector<GURL> FaviconBackendWrapper::GetCachedRecentRedirectsForPage(
    const GURL& page_url) {
  // By only returning |page_url| this code won't set the favicon on redirects.
  // If that becomes necessary, we would need this class to know about
  // redirects. Chrome does this by way of HistoryService remembering redirects
  // for recent pages. See |HistoryBackend::recent_redirects_|.
  return {page_url};
}

FaviconBackendWrapper::~FaviconBackendWrapper() = default;

void FaviconBackendWrapper::ScheduleCommit() {
  if (!commit_timer_.IsRunning()) {
    // 10 seconds matches that of HistoryBackend.
    commit_timer_.Start(FROM_HERE, base::Seconds(10), this,
                        &FaviconBackendWrapper::Commit);
  }
}

void FaviconBackendWrapper::Commit() {
  if (favicon_backend_)
    favicon_backend_->Commit();
}

void FaviconBackendWrapper::OnExpireTimerFired() {
  if (!favicon_backend_)
    return;

  // See comments above |kTimeDeltaForRunningExpireNoRemainingWork| for a
  // description of this logic.
  favicon::FaviconDatabase* db = favicon_backend_->db();
  auto icon_ids = db->GetFaviconsLastUpdatedBefore(
      base::Time::Now() - kTimeDeltaWhenEntriesAreRemoved,
      kMaxNumberOfEntriesToRemoveAtATime);
  for (favicon_base::FaviconID icon_id : icon_ids) {
    db->DeleteFavicon(icon_id);
    db->DeleteIconMappingsForFaviconId(icon_id);
  }
  if (!icon_ids.empty())
    Commit();
  const base::TimeDelta delta =
      icon_ids.size() == kMaxNumberOfEntriesToRemoveAtATime
          ? kTimeDeltaForRunningExpireWithRemainingWork
          : kTimeDeltaForRunningExpireNoRemainingWork;
  expire_timer_.Start(FROM_HERE, delta, this,
                      &FaviconBackendWrapper::OnExpireTimerFired);
}

}  // namespace weblayer
