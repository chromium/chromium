// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/traffic_annotation/network_traffic_annotation.h"

#include "dummy_classes.h"

// This file provides samples for testing the extractor.py script.

namespace {

net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("id1", R"(
        semantics {
          sender: "sender1"
          description: "desc1"
          trigger: "trigger1"
          data: "data1"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "setting1"
          chrome_policy {
            SpellCheckServiceEnabled {
                SpellCheckServiceEnabled: false
            }
          }
        }
        comments: "comment1")");
}

void TestAnnotations() {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("id2", "completing_id2", R"(
        semantics {
          sender: "sender2"
          description: "desc2"
          trigger: "trigger2"
          data: "data2"
          destination: WEBSITE
        })");

  net::NetworkTrafficAnnotationTag completed_traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("id3", partial_traffic_annotation,
                                            R"(
        policy {
          cookies_allowed: YES
          cookie_store: "user"
          setting: "setting3"
          chrome_policy {
            SpellCheckServiceEnabled {
                SpellCheckServiceEnabled: false
            }
          }
        }
        comments: "comment3")");

  net::NetworkTrafficAnnotationTag completed_branch_traffic_annotation =
      net::BranchedCompleteNetworkTrafficAnnotation(
          "id4", "branch4", partial_traffic_annotation, R"(
        policy {
          cookies_allowed: YES
          cookie_store: "user"
          setting: "setting4"
          policy_exception_justification: "justification"
        })");
}

void TestURLFetcherCreate() {
  net::URLFetcherDelegate* delegate = nullptr;
  net::URLFetcher::Create(GURL(), net::URLFetcher::RequestType::TEST_VALUE,
                          delegate);

  net::URLFetcher::Create(0, GURL(), net::URLFetcher::RequestType::TEST_VALUE,
                          delegate);

  net::URLFetcher::Create(GURL(), net::URLFetcher::RequestType::TEST_VALUE,
                          delegate, kTrafficAnnotation);

  net::URLFetcher::Create(0, GURL(), net::URLFetcher::RequestType::TEST_VALUE,
                          delegate, NO_TRAFFIC_ANNOTATION_YET);

  net::URLFetcher::Create(GURL(), net::URLFetcher::RequestType::TEST_VALUE,
                          delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  SetPartialNetworkTrafficAnnotation(PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);
}

void TestCreateRequest() {
  net::URLRequest::Delegate* delegate = nullptr;
  net::URLRequestContext context;

  context.CreateRequest(GURL(), net::RequestPriority::TEST_VALUE, delegate);
  context.CreateRequest(GURL(), net::RequestPriority::TEST_VALUE, delegate,
                        kTrafficAnnotation);
}

void TestInitList() {
  net::NetworkTrafficAnnotationTag({-1});
  net::MutableNetworkTrafficAnnotationTag({-2});
  net::PartialNetworkTrafficAnnotationTag({-1});
  net::MutablePartialNetworkTrafficAnnotationTag({-2});
  int i = 0;
  net::NetworkTrafficAnnotationTag({i});
}

void TestAssignment() {
  net::MutableNetworkTrafficAnnotationTag tag1;
  tag1.unique_id_hash_code = 1;

  net::MutablePartialNetworkTrafficAnnotationTag tag2;
  tag2.unique_id_hash_code = 2;

  // Test if assignment to |unique_id_hash_code| of another structure is not
  // caught.
  struct something_else {
    int unique_id_hash_code;
  } x;

  x.unique_id_hash_code = 3;
}

void TestMutableTags() {
  SetAnnotationTagForSomething(CreateMutableNetworkTrafficAnnotationTag(3));
}

void DummyFunction(net::NetworkTrafficAnnotationTag traffic_annotation) {}

void TestMacroExpansion() {
  DummyFunction(NO_TRAFFIC_ANNOTATION_YET);
}
