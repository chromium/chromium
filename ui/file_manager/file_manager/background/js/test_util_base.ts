// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
window.test = window.test || {};
const test = window.test;

/**
 * Namespace for test utility functions.
 *
 * Public functions in the test.util.sync and the test.util.async namespaces are
 * published to test cases and can be called by using callRemoteTestUtil. The
 * arguments are serialized as JSON internally. If application ID is passed to
 * callRemoteTestUtil, the content window of the application is added as the
 * first argument. The functions in the test.util.async namespace are passed the
 * callback function as the last argument.
 */
test.util = {};

/**
 * Namespace for synchronous utility functions.
 */
test.util.sync = {};

/**
 * Namespace for asynchronous utility functions.
 */
test.util.async = {};

export {test};
