// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/traffic_annotation/network_traffic_annotation.h"

// This file includes a template and some samples for text-coded
// traffic_annotation. For more description on each field, please refer to:
// chrome/browser/privacy/traffic_annotation.proto
// and
// out/Debug/gen/components/policy/proto/chrome_settings.proto
// For more information on policies, please refer to:
// https://cloud.google.com/docs/chrome-enterprise/policies

// A template for one level traffic annotation.
void network_traffic_annotation_template() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("...", R"(
        semantics {
          sender: "..."
          description: "..."
          trigger: "..."
          data: "..."
          destination: WEBSITE/GOOGLE_OWNED_SERVICE/OTHER
          internal {
            contacts {
              email: "..."
            }
            contacts {
              email: "..."
            }
          }
          user_data {
            type: ...
          }
          last_reviewed: "YYYY-MM-DD"
        }
        policy {
          cookies_allowed: NO/YES
          cookies_store: "..."
          setting: "..."
          chrome_policy {
            [POLICY_NAME] {
                [POLICY_NAME]: ...
            }
          }
          policy_exception_justification = "..."
        }
        comments: "..."
      )");
}

// An example on one level traffic annotation.
void network_traffic_annotation_sample() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("spellcheck_lookup", R"(
        semantics {
          sender: "Online Spellcheck"
          description:
            "Chrome can provide smarter spell-checking by sending text you "
            "type into the browser to Google's servers, allowing you to use "
            "the same spell-checking technology used by Google products, such "
            "as Docs. If the feature is enabled, Chrome will send the entire "
            "contents of text fields as you type in them to Google along with "
            "the browser’s default language. Google returns a list of "
            "suggested spellings, which will be displayed in the context menu."
          trigger: "User types text into a text field or asks to correct a "
                   "misspelled word."
          data: "Text a user has typed into a text field. No user identifier "
                "is sent along with the text."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "john-doe@chromium.org"
            }
            contacts {
              email: "spellcheck-team@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
          }
          last_reviewed: "2023-01-01"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Use a web service to "
            "help resolve spelling errors.' in Chrome's settings under "
            "Advanced. The feature is disabled by default."
          chrome_policy {
            SpellCheckServiceEnabled {
                SpellCheckServiceEnabled: false
            }
          }
        })");
}

// Example for Nx1 partial annotations where the partial annotations are defined
// in PrefetchImage1 and PrefetchImage2, and the completing annotation is
// defined in GetBitmap. Partial annotations are missing cookies fields and are
// completed in GetBitmap function.
void PrefetchImage1(const GURL& url) {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("omnibox_prefetch_image",
                                                 "bitmap_fetcher",
                                                 R"(
        semantics {
          sender: "Omnibox"
          description:
            "Chromium provides answers in the suggestion list for certain "
            "queries that the user types in the omnibox. This request "
            "retrieves a small image (for example, an icon illustrating the "
            "current weather conditions) when this can add information to an "
            "answer."
          trigger:
            "Change of results for the query typed by the user in the "
            "omnibox."
          data:
            "The only data sent is the path to an image. No user data is "
            "included, although some might be inferrable (e.g. whether the "
            "weather is sunny or rainy in the user's current location) from "
            "the name of the image in the path."
          destination: WEBSITE
          internal {
            contacts {
              email: "john-doe@chromium.org"
            }
            contacts {
              email: "omnibox-team@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-01-01"
        }
        policy {
          setting:
            "You can enable or disable this feature via 'Use a prediction "
            "service to help complete searches and URLs typed in the "
            "address bar.' in Chromium's settings under Advanced. The "
            "feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                SearchSuggestEnabled: false
            }
          }
        })");
  GetBitmap(url, partial_traffic_annotation);
}

void PrefetchImage2(const GURL& url) {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("credential_avatar",
                                                 "bitmap_fetcher",
                                                 R"(
        semantics {
          sender: "Chrome Password Manager"
          description:
            "Every credential saved in Chromium via the Credential Management "
            "API can have an avatar URL. The URL is essentially provided by "
            "the site calling the API. The avatar is used in the account "
            "chooser UI and auto signin toast which appear when a site calls "
            "navigator.credentials.get(). The avatar is retrieved before "
            "showing the UI."
          trigger:
            "User visits a site that calls navigator.credentials.get(). "
            "Assuming there are matching credentials in the Chromium password "
            "store, the avatars are retrieved."
          destination: WEBSITE
          internal {
            contacts {
              email: "john-doe@chromium.org"
            }
            contacts {
              email: "password-manager-team@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
            type: ACCESS_TOKEN
          }
          last_reviewed: "2023-01-01"
        }
        policy {
          setting:
            "One can disable saving new credentials in the settings (see "
            "'Passwords and forms'). There is no setting to disable the API."
          chrome_policy {
            PasswordManagerEnabled {
                PasswordManagerEnabled: false
            }
          }
        })");
  GetBitmap(url, partial_traffic_annotation);
}

void GetBitmap(
    const GURL& url,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("bitmap_fetcher",
                                            partial_traffic_annotation,
                                            R"(
              policy {
                cookies_allowed: YES
                cookies_store: "user"
              })");
  url_fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this,
                                         traffic_annotation);
  ...
}

// Example for 1xN partial annoations where the partial annotation is defined in
// UploadLog and two completing annotations are defined
// in GetNetworkTrafficAnnotation. The partial annotation is missing |sender|
// and |description| fields and is completed in GetNetworkTrafficAnnotation.
net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const bool& uma_service_type,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  if (uma_service_type) {
    return net::BranchedCompleteNetworkTrafficAnnotation(
        "metrics_report_uma", "metrics_report_based_on_service_type",
        partial_traffic_annotation, R"(
        semantics {
          sender: "Metrics UMA Log Uploader"
          description:
            "Report of usage statistics and crash-related data about Chromium. "
            "Usage statistics contain information such as preferences, button "
            "clicks, and memory usage and do not include web page URLs or "
            "personal information. See more at "
            "https://www.google.com/chrome/browser/privacy/ under 'Usage "
            "statistics and crash reports'. Usage statistics are tied to a "
            "pseudonymous machine identifier and not to your email address."
        })");
  } else {
    return net::BranchedCompleteNetworkTrafficAnnotation(
        "metrics_report_ukm", "metrics_report_based_on_service_type",
        partial_traffic_annotation, R"(
        semantics {
          sender: "Metrics UKM Log Uploader"
          description:
            "Report of usage statistics that are keyed by URLs to Chromium, "
            "sent only if the profile has History Sync. This includes "
            "information about the web pages you visit and your usage of them, "
            "such as page load speed. This will also include URLs and "
            "statistics related to downloaded files. If Extension Sync is "
            "enabled, these statistics will also include information about "
            "the extensions that have been installed from Chrome Web Store. "
            "Google only stores usage statistics associated with published "
            "extensions, and URLs that are known by Google’s search index. "
            "Usage statistics are tied to a pseudonymous machine identifier "
            "and not to your email address."
        })");
  }
}

void UploadLog(const bool& uma_service_type) {
  const GURL &url,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
          net::DefinePartialNetworkTrafficAnnotation(
              "net_metrics_report", "metrics_report_based_on_service_type",
              R"(
        semantics {
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chromium is running."
          data:
            "A protocol buffer with usage statistics and crash related data."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "john-doe@chromium.org"
            }
            contacts {
              email: "metrics-team@google.com"
            }
          }
          user_data {
            type: OTHER
          }
          last_reviewed: "2023-01-01"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by disabling "
            "'Automatically send usage statistics and crash reports to Google' "
            "in Chromium's settings under Advanced Settings, Privacy. The "
            "feature is enabled by default."
          chrome_policy {
            MetricsReportingEnabled {
              MetricsReportingEnabled: false
            }
          }
        })");

  current_fetch_ = net::URLFetcher::Create(
      url, net::URLFetcher::POST, this,
      GetNetworkTrafficAnnotation(uma_service_type,
                                  partial_traffic_annotation));

  ...
}
