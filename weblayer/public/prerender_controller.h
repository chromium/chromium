// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_PRERENDER_CONTROLLER_H_
#define WEBLAYER_PUBLIC_PRERENDER_CONTROLLER_H_

class GURL;

namespace weblayer {

// PrerenderController enables prerendering of urls.
// Prerendering has the same effect as adding a link rel="prerender" resource
// hint to a web page. It is implemented using NoStatePrefetch and fetches
// resources needed for a url in advance, but does not execute Javascript or
// render any part of the page in advance. For more information on
// NoStatePrefetch, see
// https://developers.google.com/web/updates/2018/07/nostate-prefetch.
class PrerenderController {
 public:
  virtual void Prerender(const GURL& url) = 0;
  virtual void DestroyAllContents() = 0;

 protected:
  virtual ~PrerenderController() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_PRERENDER_CONTROLLER_H_
