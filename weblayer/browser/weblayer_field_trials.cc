// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_field_trials.h"

#include "base/path_service.h"
#include "components/metrics/persistent_histograms.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

void WebLayerFieldTrials::OnVariationsSetupComplete() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath metrics_dir;
  if (base::PathService::Get(DIR_USER_DATA, &metrics_dir)) {
    InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  } else {
    NOTREACHED();
  }
}

}  // namespace weblayer
