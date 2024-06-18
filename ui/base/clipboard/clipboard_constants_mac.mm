// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

#import <Foundation/Foundation.h>

namespace ui {

NSString* const kUTTypeChromiumImageAndHTML = @"org.chromium.image-html";

NSString* const kUTTypeChromiumInitiatedDrag =
    @"org.chromium.chromium-initiated-drag";

NSString* const kUTTypeChromiumPrivilegedInitiatedDrag =
    @"org.chromium.chromium-privileged-initiated-drag";

NSString* const kUTTypeChromiumRendererInitiatedDrag =
    @"org.chromium.chromium-renderer-initiated-drag";

NSString* const kUTTypeChromiumDataTransferCustomData =
    @"org.chromium.web-custom-data";

NSString* const kUTTypeConfidentialData = @"org.nspasteboard.ConcealedType";

// This isn't just a "publicly-used type", but genuinely used to be a real
// supported type that somehow disappeared. It used to be available in Carbon as
// the 'urln' as part of a pair with 'url ', and in fact was documented as such
// in the original 2009 "Uniform Type Identifiers Reference" PDF from Apple (and
// which now lives on on random PDF archiving websites such as
// https://sbgsmedia.in/2014/08/28/362ed8bfea200a683234e28c31b8b2bd.pdf). This
// UTType constant is still used by WebKit; see
// https://github.com/WebKit/WebKit/search?q=WebURLNamePboardType
NSString* const kUTTypeURLName = @"public.url-name";

// aka "NeXT smart paste pasteboard type". This constant is still used by
// WebKit; see https://github.com/WebKit/WebKit/search?q=WebSmartPastePboardType
NSString* const kUTTypeWebKitWebSmartPaste =
    @"dyn.ah62d4rv4gu8y63n2nuuhg5pbsm4ca6dbsr4gnkduqf31k3pcr7u1e3basv61a3k";

// aka "WebURLsWithTitlesPboardType". This constant is still used by WebKit; see
// https://github.com/WebKit/WebKit/search?q=WebURLsWithTitlesPboardType
NSString* const kUTTypeWebKitWebURLsWithTitles =
    @"dyn.ah62d4rv4gu8zs3pcnzme2641rf4guzdmsv0gn64uqm10c6xenv61a3k";

NSString* const kUTTypeChromiumSourceURL = @"org.chromium.source-url";

}  // namespace ui
