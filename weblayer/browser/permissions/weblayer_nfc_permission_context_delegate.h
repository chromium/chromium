// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_NFC_PERMISSION_CONTEXT_DELEGATE_H_
#define WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_NFC_PERMISSION_CONTEXT_DELEGATE_H_

#include "build/build_config.h"
#include "components/permissions/contexts/nfc_permission_context.h"

namespace weblayer {

class WebLayerNfcPermissionContextDelegate
    : public permissions::NfcPermissionContext::Delegate {
 public:
  WebLayerNfcPermissionContextDelegate();

  WebLayerNfcPermissionContextDelegate(
      const WebLayerNfcPermissionContextDelegate&) = delete;
  WebLayerNfcPermissionContextDelegate& operator=(
      const WebLayerNfcPermissionContextDelegate&) = delete;

  ~WebLayerNfcPermissionContextDelegate() override;

  // NfcPermissionContext::Delegate:
#if BUILDFLAG(IS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_NFC_PERMISSION_CONTEXT_DELEGATE_H_