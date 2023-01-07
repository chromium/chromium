/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Standalone script to be included in the relay-document
 * used by goog.net.xpc.IframeRelayTransport. This script will decode the
 * fragment identifier, determine the target window object and deliver
 * the data to it.
 */

goog.provide('goog.net.xpc.relay');

(function() {
'use strict';
// Decode the fragement identifier.
// location.href is expected to be structured as follows:
// <url>#<channel_name>[,<iframe_id>]|<data>

// Get the fragment identifier.
let raw = window.location.hash;
if (!raw) {
  return;
}
if (raw.charAt(0) == '#') {
  raw = raw.substring(1);
}
const pos = raw.indexOf('|');
const head = raw.substring(0, pos).split(',');
const channelName = head[0];
const iframeId = head.length == 2 ? head[1] : null;
const frame = raw.substring(pos + 1);

// Find the window object of the peer.
//
// The general structure of the frames looks like this:
// - peer1
//   - relay2
//   - peer2
//     - relay1
//
// We are either relay1 or relay2.

let win;
if (iframeId) {
  // We are relay2 and need to deliver the data to peer2.
  win = window.parent.frames[iframeId];
} else {
  // We are relay1 and need to deliver the data to peer1.
  win = window.parent.parent;
}

// Deliver the data.
try {
  win['xpcRelay'](channelName, frame);
} catch (e) {
  // Nothing useful can be done here.
  // It would be great to inform the sender the delivery of this message
  // failed, but this is not possible because we are already in the receiver's
  // domain at this point.
}
})();
