// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_CONTENT_CLIENT_H_
#define WOLVIC_WOLVIC_CONTENT_CLIENT_H_

#include <string>

#include "content/public/common/content_client.h"

namespace wolvic {

class WolvicContentClient : public content::ContentClient {
 public:
  WolvicContentClient();
  ~WolvicContentClient() override;

  std::u16string GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override;
  media::MediaDrmBridgeClient* GetMediaDrmBridgeClient() override;
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_CONTENT_CLIENT_H_
