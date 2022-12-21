// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.IWebEngineDelegate;
import org.chromium.webengine.interfaces.IWebFragmentEventsDelegate;

oneway interface IWebEngineDelegateClient {
    void onDelegatesReady(
            IWebEngineDelegate delegate,
            IWebFragmentEventsDelegate fragmentEventsDelegate,
            ITabManagerDelegate tabManagerDelegate,
            ICookieManagerDelegate cookieManagerDelegate) = 1;
}
