// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File is added in safe list.

// An annotation with last_reviewed date but
// missing email address, unspecified user_data
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("partially_populated_safe_listed", R"(
  semantics {
    sender: "Cloud Policy"
    description:
    "Used to fetch policy for extensions, policy-controlled wallpaper, "
    "and custom terms of service."
    trigger:
    "Periodically loaded when a managed user is signed in to Chrome."
    data:
    "This request does not send any data. It loads external resources "
    "by a unique URL provided by the admin."
    destination: GOOGLE_OWNED_SERVICE
    last_reviewed: "2022-12-23"
  }
  policy {
    cookies_allowed: NO
    setting:
    "This feature cannot be controlled by Chrome settings, but users "
    "can sign out of Chrome to disable it."
    policy_exception_justification:
    "Not implemented, considered not useful. This request is part of "
    "the policy fetcher itself."
  })");

// incomplete annotation without email address, unspecified user_data,
// last_reviewed
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("missing_all_new_field_safe_listed", R"(
  semantics {
    sender: "Cloud Policy"
    description:
    "Used to fetch policy for extensions, policy-controlled wallpaper, "
    "and custom terms of service."
    trigger:
    "Periodically loaded when a managed user is signed in to Chrome."
    data:
    "This request does not send any data. It loads external resources "
    "by a unique URL provided by the admin."
    destination: GOOGLE_OWNED_SERVICE
  }
  policy {
    cookies_allowed: NO
    setting:
    "This feature cannot be controlled by Chrome settings, but users "
    "can sign out of Chrome to disable it."
    policy_exception_justification:
    "Not implemented, considered not useful. This request is part of "
    "the policy fetcher itself."
  })");

// Complete annotation
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("ok_new_fields_safe_listed", R"(
  semantics {
    sender: "Cloud Policy"
    description:
    "Used to fetch policy for extensions, policy-controlled wallpaper, "
    "and custom terms of service."
    trigger:
    "Periodically loaded when a managed user is signed in to Chrome."
    data:
    "This request does not send any data. It loads external resources "
    "by a unique URL provided by the admin."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts {
        email: "chromium-dev@google.com"
      }
    }
    user_data {
      type: NONE
    }
    last_reviewed: "2022-12-23"
  }
  policy {
    cookies_allowed: NO
    setting:
    "This feature cannot be controlled by Chrome settings, but users "
    "can sign out of Chrome to disable it."
    policy_exception_justification:
    "Not implemented, considered not useful. This request is part of "
    "the policy fetcher itself."
  })");
