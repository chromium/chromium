// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_COMMON_CONTENT_CLIENT_H_
#define UI_WEBUI_EXAMPLES_COMMON_CONTENT_CLIENT_H_

#include <string_view>

#include "content/public/common/content_client.h"

namespace webui_examples {

class ContentClient : public content::ContentClient {
 public:
  ContentClient();
  ContentClient(const ContentClient&) = delete;
  ContentClient& operator=(const ContentClient&) = delete;
  ~ContentClient() override;

 private:
  // content::ContentClient:
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_COMMON_CONTENT_CLIENT_H_
