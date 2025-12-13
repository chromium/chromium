// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary SendPacketOptions {
  // Target IP address.
  required DOMString ip;
  // Packet time to live value. If omitted, the system default value will be
  // used.
  long ttl;
  // Packet timeout in seconds. If omitted, the system default value will be
  // used.
  long timeout;
  // Size of the payload. If omitted, the system default value will be used.
  long size;
};

dictionary SendPacketResult {
  // The IP of the host which we receives the ICMP reply from.
  // The IP may differs from our target IP if the packet's ttl is used up.
  required DOMString ip;

  // Latency in millisenconds.
  required double latency;
};

// Use the <code>chrome.diagnostics</code> API to query various properties of
// the environment that may be useful for diagnostics.
interface Diagnostics {
  // Send a packet of the given type with the given parameters.
  // |PromiseValue|: result
  [requiredCallback] static Promise<SendPacketResult> sendPacket(
      SendPacketOptions options);
};

partial interface Browser {
  static attribute Diagnostics diagnostics;
};
