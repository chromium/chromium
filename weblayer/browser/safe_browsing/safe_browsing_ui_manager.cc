// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace weblayer {

SafeBrowsingUIManager::SafeBrowsingUIManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(timvolodine): figure out if we want to send any threat reporting here.
  // Note the base implementation does not send anything.
}

}  // namespace weblayer
