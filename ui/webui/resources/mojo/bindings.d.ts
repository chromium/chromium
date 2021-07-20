// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimal definitions for the Mojo core bindings, just enough to make the TS
// compiler to not throw errors. These should be fleshed out, or even better
// auto-generated from bindings_uncompiled.js eventually. The latter currently
// does not produce definitions that work.

export namespace mojo {
  namespace internal {
    namespace interfaceSupport {

      interface Endpoint {}

      interface PendingReceiver {
        readonly handle: Endpoint;
      }

      function getEndpointForReceiver(handle: Endpoint): Endpoint;
    }
  }
}
