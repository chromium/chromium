// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/common/content_client.h"

#include <string_view>

#include "ui/base/resource/resource_bundle.h"

namespace webui_examples {

ContentClient::ContentClient() = default;
ContentClient::~ContentClient() = default;

std::string_view ContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ContentClient::GetDataResourceBytes(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

}  // namespace webui_examples
