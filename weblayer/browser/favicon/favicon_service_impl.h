// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/favicon/core/core_favicon_service.h"

namespace base {
class FilePath;
}

namespace weblayer {

class FaviconBackendWrapper;
class FaviconServiceImplObserver;

// FaviconServiceImpl provides the front end (ui side) access to the favicon
// database. Most functions are processed async on the backend task-runner.
class FaviconServiceImpl : public favicon::CoreFaviconService {
 public:
  FaviconServiceImpl();
  FaviconServiceImpl(const FaviconServiceImpl&) = delete;
  FaviconServiceImpl& operator=(const FaviconServiceImpl&) = delete;
  ~FaviconServiceImpl() override;

  void Init(const base::FilePath& db_path);

  void set_observer(FaviconServiceImplObserver* observer) {
    observer_ = observer;
  }

  // Deletes the database and recreates it, notifying |callback| when done.
  void DeleteAndRecreateDatabase(base::OnceClosure callback);

  // Requests the favicon image for a url (page). The returned image matches
  // that returned from FaviconFetcher.
  base::CancelableTaskTracker::TaskId GetFaviconForPageUrl(
      const GURL& page_url,
      base::OnceCallback<void(gfx::Image)> callback,
      base::CancelableTaskTracker* tracker);
  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types,
      int minimum_size_in_pixels,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker);

  // favicon::CoreFaviconService:
  base::CancelableTaskTracker::TaskId GetFaviconForPageURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
  void SetFaviconOutOfDateForPage(const GURL& page_url) override;
  void SetFavicons(const base::flat_set<GURL>& page_urls,
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   const gfx::Image& image) override;
  void CloneFaviconMappingsForPages(
      const GURL& page_url_to_read,
      const favicon_base::IconTypeSet& icon_types,
      const base::flat_set<GURL>& page_urls_to_write) override;
  base::CancelableTaskTracker::TaskId GetFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId UpdateFaviconMappingsAndFetch(
      const base::flat_set<GURL>& page_urls,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
  void DeleteFaviconMappings(const base::flat_set<GURL>& page_urls,
                             favicon_base::IconType icon_type) override;
  void UnableToDownloadFavicon(const GURL& icon_url) override;
  void ClearUnableToDownloadFavicons() override;
  bool WasUnableToDownloadFavicon(const GURL& icon_url) const override;

 private:
  using MissingFaviconUrlHash = size_t;
  SEQUENCE_CHECKER(sequence_checker_);

  // Returns the desired favicon sizes for the current platform.
  static std::vector<int> GetDesiredFaviconSizesInPixels();

  // The TaskRunner to which FaviconServiceBackend tasks are posted. Nullptr
  // once Cleanup() is called.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  scoped_refptr<FaviconBackendWrapper> backend_;

  // Hashes of the favicon urls that were unable to be downloaded.
  std::unordered_set<MissingFaviconUrlHash> missing_favicon_urls_;

  // This is only used in tests, where only a single observer is necessary.
  raw_ptr<FaviconServiceImplObserver> observer_ = nullptr;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_H_
