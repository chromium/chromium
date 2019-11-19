// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/content_client_impl.h"

#include "build/build_config.h"
#include "content/app/resources/grit/content_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace weblayer {

ContentClientImpl::ContentClientImpl() {}

ContentClientImpl::~ContentClientImpl() {}

base::string16 ContentClientImpl::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

base::string16 ContentClientImpl::GetLocalizedString(
    int message_id,
    const base::string16& replacement) {
  return l10n_util::GetStringFUTF16(message_id, replacement);
}

base::StringPiece ContentClientImpl::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ContentClientImpl::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& ContentClientImpl::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace weblayer
