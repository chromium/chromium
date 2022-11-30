// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/weblayer_nfc_permission_context_delegate.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/tab_impl.h"
#endif

namespace weblayer {

WebLayerNfcPermissionContextDelegate::WebLayerNfcPermissionContextDelegate() =
    default;

WebLayerNfcPermissionContextDelegate::~WebLayerNfcPermissionContextDelegate() =
    default;

#if BUILDFLAG(IS_ANDROID)
bool WebLayerNfcPermissionContextDelegate::IsInteractable(
    content::WebContents* web_contents) {
  auto* tab = TabImpl::FromWebContents(web_contents);
  return tab && tab->IsActive();
}
#endif

}  // namespace weblayer
