// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRDTP_JSON_H_
#define CRDTP_JSON_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "export.h"
#include "json_platform.h"
#include "parser_handler.h"

namespace crdtp {
namespace json {
// =============================================================================
// json::NewJSONEncoder - for encoding streaming parser events as JSON
// =============================================================================

// Returns a handler object which will write ascii characters to |out|.
// |status->ok()| will be false iff the handler routine HandleError() is called.
// In that case, we'll stop emitting output.
// Except for calling the HandleError routine at any time, the client
// code must call the Handle* methods in an order in which they'd occur
// in valid JSON; otherwise we may crash (the code uses assert).
CRDTP_EXPORT std::unique_ptr<StreamingParserHandler> NewJSONEncoder(
    const Platform* platform,
    std::vector<uint8_t>* out,
    Status* status);
CRDTP_EXPORT std::unique_ptr<StreamingParserHandler>
NewJSONEncoder(const Platform* platform, std::string* out, Status* status);

// =============================================================================
// json::ParseJSON - for receiving streaming parser events for JSON
// =============================================================================

CRDTP_EXPORT void ParseJSON(const Platform& platform,
                            span<uint8_t> chars,
                            StreamingParserHandler* handler);
CRDTP_EXPORT void ParseJSON(const Platform& platform,
                            span<uint16_t> chars,
                            StreamingParserHandler* handler);

// =============================================================================
// json::ConvertCBORToJSON, json::ConvertJSONToCBOR - for transcoding
// =============================================================================
CRDTP_EXPORT Status ConvertCBORToJSON(const Platform& platform,
                                      span<uint8_t> cbor,
                                      std::string* json);
CRDTP_EXPORT Status ConvertCBORToJSON(const Platform& platform,
                                      span<uint8_t> cbor,
                                      std::vector<uint8_t>* json);
CRDTP_EXPORT Status ConvertJSONToCBOR(const Platform& platform,
                                      span<uint8_t> json,
                                      std::vector<uint8_t>* cbor);
CRDTP_EXPORT Status ConvertJSONToCBOR(const Platform& platform,
                                      span<uint16_t> json,
                                      std::vector<uint8_t>* cbor);
CRDTP_EXPORT Status ConvertJSONToCBOR(const Platform& platform,
                                      span<uint8_t> json,
                                      std::string* cbor);
CRDTP_EXPORT Status ConvertJSONToCBOR(const Platform& platform,
                                      span<uint16_t> json,
                                      std::string* cbor);
}  // namespace json
}  // namespace crdtp

#endif  // CRDTP_JSON_H_
