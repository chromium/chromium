// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_service_impl.h"

#include <stddef.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "content/public/common/url_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"
#include "weblayer/browser/favicon/favicon_backend_wrapper.h"
#include "weblayer/browser/favicon/favicon_service_impl_observer.h"

namespace weblayer {
namespace {

bool CanAddUrl(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(url::kJavaScriptScheme) || url.SchemeIs(url::kAboutScheme) ||
      url.SchemeIs(url::kContentScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kViewSourceScheme)) {
    return false;
  }

  return true;
}

// Returns the IconTypeSet for the current platform. This matches the set
// of favicon types that are requested for the platform (see
// FaviconDriverImpl).
favicon_base::IconTypeSet GetIconTypeSet() {
#if BUILDFLAG(IS_ANDROID)
  return {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
          favicon_base::IconType::kTouchPrecomposedIcon,
          favicon_base::IconType::kWebManifestIcon};
#else
  return {favicon_base::IconType::kFavicon};
#endif
}

int GetDesiredFaviconSizeInDips() {
#if BUILDFLAG(IS_ANDROID)
  // This is treatest as the largest available icon.
  return 0;
#else
  return gfx::kFaviconSize;
#endif
}

void OnGotFaviconsForPageUrl(
    int desired_size_in_dip,
    base::OnceCallback<void(gfx::Image)> callback,
    std::vector<favicon_base::FaviconRawBitmapResult> results) {
  favicon_base::FaviconImageResult image_result;
  image_result.image = favicon_base::SelectFaviconFramesFromPNGs(
      results, favicon_base::GetFaviconScales(), desired_size_in_dip);
  favicon_base::SetFaviconColorSpace(&image_result.image);
  std::move(callback).Run(image_result.image);
}

}  // namespace

FaviconServiceImpl::FaviconServiceImpl() = default;

FaviconServiceImpl::~FaviconServiceImpl() {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::Shutdown, std::move(backend_)));
}

void FaviconServiceImpl::Init(const base::FilePath& db_path) {
  if (!backend_task_runner_) {
    // BLOCK_SHUTDOWN matches that of HistoryService. It's done in hopes of
    // preventing database corruption.
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::WithBaseSyncPrimitives(),
         base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  backend_ = base::MakeRefCounted<FaviconBackendWrapper>(backend_task_runner_);

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::Init, backend_, db_path));
}

void FaviconServiceImpl::DeleteAndRecreateDatabase(base::OnceClosure callback) {
  backend_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::DeleteAndRecreateDatabase,
                     backend_),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetFaviconForPageUrl(
    const GURL& page_url,
    base::OnceCallback<void(gfx::Image)> callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The arguments supplied to this function should return an image matching
  // that returned by FaviconFetcher.
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::GetFaviconsForUrl, backend_,
                     page_url, GetIconTypeSet(),
                     GetDesiredFaviconSizesInPixels()),
      base::BindOnce(&OnGotFaviconsForPageUrl, GetDesiredFaviconSizeInDips(),
                     std::move(callback)));
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::GetLargestRawFaviconForPageURL(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types,
    int minimum_size_in_pixels,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::GetLargestFaviconForUrl, backend_,
                     page_url, icon_types, minimum_size_in_pixels),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetFaviconForPageURL(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    int desired_size_in_dip,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::GetFaviconsForUrl, backend_,
                     page_url, icon_types,
                     GetPixelSizesForFaviconScales(desired_size_in_dip)),
      std::move(callback));
}

void FaviconServiceImpl::SetFaviconOutOfDateForPage(const GURL& page_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::SetFaviconsOutOfDateForPage,
                     backend_, page_url));
}

void FaviconServiceImpl::SetFavicons(const base::flat_set<GURL>& page_urls,
                                     const GURL& icon_url,
                                     favicon_base::IconType icon_type,
                                     const gfx::Image& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<GURL> page_urls_to_save;
  page_urls_to_save.reserve(page_urls.capacity());
  for (const GURL& page_url : page_urls) {
    if (CanAddUrl(page_url))
      page_urls_to_save.insert(page_url);
  }

  if (page_urls_to_save.empty())
    return;

  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FaviconBackendWrapper::SetFavicons, backend_,
                                page_urls_to_save, icon_type, icon_url,
                                ExtractSkBitmapsToStore(image)));
}

void FaviconServiceImpl::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::CloneFaviconMappingsForPages,
                     backend_, page_url_to_read, icon_types,
                     page_urls_to_write));
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    int desired_size_in_dip,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::GetFavicon, backend_, icon_url,
                     icon_type,
                     GetPixelSizesForFaviconScales(desired_size_in_dip)),
      std::move(callback));
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    int desired_size_in_dip,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FaviconBackendWrapper::UpdateFaviconMappingsAndFetch,
                     backend_, page_urls, icon_url, icon_type,
                     GetPixelSizesForFaviconScales(desired_size_in_dip)),
      std::move(callback));
}

void FaviconServiceImpl::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FaviconBackendWrapper::DeleteFaviconMappings,
                                backend_, page_urls, icon_type));
}

void FaviconServiceImpl::UnableToDownloadFavicon(const GURL& icon_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  missing_favicon_urls_.insert(base::FastHash(icon_url.spec()));
  if (observer_)
    observer_->OnUnableToDownloadFavicon();
}

void FaviconServiceImpl::ClearUnableToDownloadFavicons() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  missing_favicon_urls_.clear();
}

bool FaviconServiceImpl::WasUnableToDownloadFavicon(
    const GURL& icon_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MissingFaviconUrlHash url_hash = base::FastHash(icon_url.spec());
  return missing_favicon_urls_.find(url_hash) != missing_favicon_urls_.end();
}

// static
std::vector<int> FaviconServiceImpl::GetDesiredFaviconSizesInPixels() {
  return GetPixelSizesForFaviconScales(GetDesiredFaviconSizeInDips());
}

}  // namespace weblayer
