// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An OK annotation
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("ok_annotation", R"(
  semantics {
    sender: "Cloud Policy"
    description:
    "Used to fetch policy for extensions, policy-controlled wallpaper, "
    "and custom terms of service."
    trigger:
    "Periodically loaded when a managed user is signed in to Chrome."
    internal  {
      contacts {
        email: "crmullins@google.com"
      }
      contacts {
        owners: "//tools/traffic_annotation/OWNERS"
      }
    }
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

// An OK annotation with owner instead of email as contact
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("ok_annotation_only_owner", R"(
  semantics {
    sender: "Cloud Policy"
    description:
    "Used to fetch policy for extensions, policy-controlled wallpaper, "
    "and custom terms of service."
    trigger:
    "Periodically loaded when a managed user is signed in to Chrome."
    user_data {
      type: NONE
    }
    internal  {
      contacts {
        owners: "//tools/traffic_annotation/OWNERS"
      }
    }
    last_reviewed: "2023-07-25"
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

// An annotation with a syntax error: semantics is missing '{'.
net::NetworkTrafficAnnotationTag traffic_annotation =
  net::DefineNetworkTrafficAnnotation("syntax_error_annotation", R"(
  semantics
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

// An annotation with a completeness error: missing sender and
// policy_exception_justification, chrome_policy field.
net::NetworkTrafficAnnotationTag traffic_annotation =
  net::DefineNetworkTrafficAnnotation("incomplete_error_annotation", R"(
  semantics {
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
    cookies_allowed: YES
    setting:
    "This feature cannot be controlled by Chrome settings, but users "
    "can sign out of Chrome to disable it."
  })");

// An annotation with a incomplete email address, unspecified user_data
// invalid date format.
net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("invalid_assignment_annotation", R"(
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
    last_reviewed: "23-12-2022"
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
