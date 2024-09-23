// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides samples for testing the extractor.py script.

/** Example class that contains a NetworkTrafficAnnotationTag definition. */
public class ValidFile {
    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "id1",
                    """
                    semantics {
                      sender: 'sender1'
                      description: 'desc1'
                      trigger: 'trigger1'
                      data: 'data1 contains \'quotes '
                      destination: GOOGLE_OWNED_SERVICE
                    }
                    policy {
                      cookies_allowed: NO
                      setting: 'setting1'
                      chrome_policy {
                        SpellCheckServiceEnabled {
                          SpellCheckServiceEnabled: false
                        }
                      }
                    }
                    comments: 'comment1'
                    """);

    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION_2 =
            NetworkTrafficAnnotationTag.createComplete(
                    "id2",
                    """
                    semantics {
                      sender: "sender1"
                      description: "desc1"
                      trigger: "trigger1 contains a backslash \\ "
                      data: "data1 contains \"quotes "
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
                    comments: "comment1"
                    """);

    private void doSomethingWith(NetworkTrafficAnnotationTag annotation) {
        // ...
    }

    public void fooBar() {
        doSomethingWith(TRAFFIC_ANNOTATION);
        doSomethingWith(TRAFFIC_ANNOTATION_2);
        doSomethingWith(NetworkTrafficAnnotationTag.NO_TRAFFIC_ANNOTATION_YET);
        doSomethingWith(NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
        doSomethingWith(NetworkTrafficAnnotationTag.TRAFFIC_ANNOTATION_FOR_TESTS);
    }
}
