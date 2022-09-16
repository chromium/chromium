// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_BACKEND_WRAPPER_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_BACKEND_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/timer/timer.h"
#include "components/favicon/core/favicon_backend_delegate.h"
#include "components/favicon_base/favicon_types.h"

class GURL;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace favicon {
class FaviconBackend;
}

namespace weblayer {

// FaviconBackendWrapper runs on a background task-runner and owns the database
// side of favicons. This class largely delegates to favicon::FaviconBackend.
class FaviconBackendWrapper
    : public base::RefCountedDeleteOnSequence<FaviconBackendWrapper>,
      public favicon::FaviconBackendDelegate {
 public:
  explicit FaviconBackendWrapper(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  FaviconBackendWrapper(const FaviconBackendWrapper&) = delete;
  FaviconBackendWrapper& operator=(const FaviconBackendWrapper&) = delete;

  void Init(const base::FilePath& db_path);

  void Shutdown();

  void DeleteAndRecreateDatabase();

  // All of these functions are called by the FaviconServiceImpl. They call
  // through to |favicon_backend_|.
  std::vector<favicon_base::FaviconRawBitmapResult> GetFaviconsForUrl(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      const std::vector<int>& desired_sizes);
  favicon_base::FaviconRawBitmapResult GetLargestFaviconForUrl(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types_list,
      int minimum_size_in_pixels);
  void SetFaviconsOutOfDateForPage(const GURL& page_url);
  void SetFavicons(const base::flat_set<GURL>& page_urls,
                   favicon_base::IconType icon_type,
                   const GURL& icon_url,
                   const std::vector<SkBitmap>& bitmaps);
  void CloneFaviconMappingsForPages(
      const GURL& page_url_to_read,
      const favicon_base::IconTypeSet& icon_types,
      const base::flat_set<GURL>& page_urls_to_write);
  std::vector<favicon_base::FaviconRawBitmapResult> GetFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const std::vector<int>& desired_sizes);
  std::vector<favicon_base::FaviconRawBitmapResult>
  UpdateFaviconMappingsAndFetch(const base::flat_set<GURL>& page_urls,
                                const GURL& icon_url,
                                favicon_base::IconType icon_type,
                                const std::vector<int>& desired_sizes);
  void DeleteFaviconMappings(const base::flat_set<GURL>& page_urls,
                             favicon_base::IconType icon_type);

  // favicon::FaviconBackendDelegate:
  std::vector<GURL> GetCachedRecentRedirectsForPage(
      const GURL& page_url) override;

 private:
  friend class base::RefCountedDeleteOnSequence<FaviconBackendWrapper>;
  friend class base::DeleteHelper<FaviconBackendWrapper>;
  friend class FaviconBackendWrapperTest;

  ~FaviconBackendWrapper() override;

  void ScheduleCommit();
  void Commit();

  // Called to expire (remove) out of date icons and restart the timer.
  void OnExpireTimerFired();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Timer used to delay commits for a short amount of time. This done to
  // batch commits.
  base::OneShotTimer commit_timer_;

  // The real implementation of the backend. If there is a problem
  // initializing the database this will be null.
  std::unique_ptr<favicon::FaviconBackend> favicon_backend_;

  // Timer used to remove items from the database that are likely no longer
  // needed.
  base::OneShotTimer expire_timer_;

  base::FilePath db_path_;
};

// These values are here only for tests.

// Amount of time before favicons are removed. That is, any favicons downloaded
// before this amount of time are removed.
constexpr base::TimeDelta kTimeDeltaWhenEntriesAreRemoved = base::Days(30);

// See comment near kMaxNumberOfEntriesToRemoveAtATime for details on this.
constexpr base::TimeDelta kTimeDeltaForRunningExpireWithRemainingWork =
    base::Minutes(2);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_BACKEND_WRAPPER_H_
