// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_OBSERVER_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_OBSERVER_H_

namespace weblayer {

class FaviconServiceImplObserver {
 public:
  // Called from FaviconServiceImpl::UnableToDownloadFavicon.
  virtual void OnUnableToDownloadFavicon() {}

 protected:
  virtual ~FaviconServiceImplObserver() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_OBSERVER_H_
