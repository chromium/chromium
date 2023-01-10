// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/common/switches.h"

namespace weblayer {
namespace switches {

// Makes WebEngine Shell use the given path for its data directory.
// NOTE: If changing this value, change the corresponding Java-side value in
// WebLayerBrowserTestsActivity.java#getUserDataDirectoryCommandLineSwitch() to
// match.
const char kWebEngineUserDataDir[] = "webengine-user-data-dir";

}  // namespace switches
}  //  namespace weblayer
