// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PAGE_LOAD_METRICS_INITIALIZE_H_
#define WEBLAYER_BROWSER_PAGE_LOAD_METRICS_INITIALIZE_H_

#include "base/callback_forward.h"

namespace content {
class WebContents;
}

namespace page_load_metrics {
class PageLoadTracker;
}

namespace weblayer {

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents);

// Sets a callback which is called by
// page_load_metrics::PageLoadMetricsEmbedderBase::RegisterEmbedderObservers.
void SetRegisterEmbedderObserversForTesting(
    base::RepeatingCallback<void(page_load_metrics::PageLoadTracker*)>*
        callback);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PAGE_LOAD_METRICS_INITIALIZE_H_
