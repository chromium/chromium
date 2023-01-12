// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File should not be added in safe list.

// Incomplete annotation
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("missing_new_fields_not_safe_listed",
                                        R"(
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

// Missing email annotation
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("missing_email_not_safe_listed", R"(
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
        email: ""
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

// Invalid user data type annotation
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("invalid_userdata_not_safe_listed", R"(
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
      type: UNSPECIFIED
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
