// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/remote_layer_api.h"

#include "base/feature_list.h"

#include <objc/runtime.h>

namespace ui {

namespace {
// Control use of cross-process CALayers to display content directly from the
// GPU process on Mac.
BASE_FEATURE(kRemoteCoreAnimationAPI,
             "RemoteCoreAnimationAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

bool RemoteLayerAPISupported() {
  if (!base::FeatureList::IsEnabled(kRemoteCoreAnimationAPI))
    return false;

  // Verify the GPU process interfaces are present.
  static Class caContextClass = NSClassFromString(@"CAContext");
  if (!caContextClass)
    return false;

  // Note that because the contextId and layer properties are dynamic,
  // instancesRespondToSelector will return NO for them.
  static bool caContextClassValid =
#if BUILDFLAG(IS_MAC)
      [caContextClass
          respondsToSelector:@selector(contextWithCGSConnection:options:)] &&
#else
      [caContextClass
          respondsToSelector:@selector(remoteContextWithOptions:)] &&
#endif
      class_getProperty(caContextClass, "contextId") &&
      class_getProperty(caContextClass, "layer");
  if (!caContextClassValid)
    return false;

  // Verify the browser process interfaces are present.
  static Class caLayerHostClass = NSClassFromString(@"CALayerHost");
  if (!caLayerHostClass)
    return false;

  static bool caLayerHostClassValid =
      [caLayerHostClass instancesRespondToSelector:@selector(contextId)] &&
      [caLayerHostClass instancesRespondToSelector:@selector(setContextId:)];
  if (!caLayerHostClassValid)
    return false;

  // If everything is there, we should be able to use the API.
  return true;
}

}  // namespace
