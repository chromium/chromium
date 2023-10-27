// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_
#define WOLVIC_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_

namespace mojo {
class BinderMap;
}  // namespace mojo

namespace wolvic {

class WolvicContentRendererClient;

void ExposeRendererInterfacesToBrowser(WolvicContentRendererClient* client,
                                       mojo::BinderMap* binders);

}  // namespace wolvic

#endif  // WOLVIC_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_
