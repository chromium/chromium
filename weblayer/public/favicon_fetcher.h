// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_FAVICON_FETCHER_H_
#define WEBLAYER_PUBLIC_FAVICON_FETCHER_H_

namespace gfx {
class Image;
}

namespace weblayer {

// FaviconFetcher is responsible for downloading a favicon for the current
// navigation. FaviconFetcher caches favicons, updating the cache every so
// often to ensure the cache is up to date.
class FaviconFetcher {
 public:
  virtual ~FaviconFetcher() = default;

  // Returns the favicon for the current navigation, which may be empty.
  virtual gfx::Image GetFavicon() = 0;

 protected:
  FaviconFetcher() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_FAVICON_FETCHER_H_
