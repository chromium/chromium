// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_XDG_SHARED_MIME_INFO_MIME_CACHE_H_
#define THIRD_PARTY_XDG_SHARED_MIME_INFO_MIME_CACHE_H_

#include <string>

namespace xdg_shared_mime_info {

// Gets the mime type (if any) that is associated with the file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeCacheTypeFromExtension(const std::string& ext, std::string* result);

}  // namespace xdg_shared_mime_info

#endif  // THIRD_PARTY_XDG_SHARED_MIME_INFO_MIME_CACHE_H_
