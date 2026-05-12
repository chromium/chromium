// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_TRAFFIC_ANNOTATION_SCRIPTS_TEST_DATA_VALID_HEADER_H_
#define TOOLS_TRAFFIC_ANNOTATION_SCRIPTS_TEST_DATA_VALID_HEADER_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

inline constexpr net::NetworkTrafficAnnotationTag kHeaderTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("header_id", R"(
        semantics {
          sender: "sender_h"
          description: "desc_h"
          trigger: "trigger_h"
          data: "data_h"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "setting_h"
          chrome_policy {
            SpellCheckServiceEnabled {
                SpellCheckServiceEnabled: false
            }
          }
        }
        comments: "comment_h")");

#endif  // TOOLS_TRAFFIC_ANNOTATION_SCRIPTS_TEST_DATA_VALID_HEADER_H_
