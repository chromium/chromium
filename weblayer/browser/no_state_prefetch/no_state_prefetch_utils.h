// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_UTILS_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_UTILS_H_

namespace content {
class WebContents;
}

namespace prerender {
class NoStatePrefetchContents;
}

namespace weblayer {

prerender::NoStatePrefetchContents* NoStatePrefetchContentsFromWebContents(
    content::WebContents* web_contents);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_UTILS_H_
