// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CONSTANTS_H_
#define URL_URL_CONSTANTS_H_

#include <stddef.h>

#include "base/component_export.h"

namespace url {

COMPONENT_EXPORT(URL) extern const char kAboutBlankURL[];
COMPONENT_EXPORT(URL) extern const char kAboutSrcdocURL[];

COMPONENT_EXPORT(URL) extern const char kAboutBlankPath[];
COMPONENT_EXPORT(URL) extern const char kAboutSrcdocPath[];

COMPONENT_EXPORT(URL) extern const char kAboutScheme[];
COMPONENT_EXPORT(URL) extern const char kBlobScheme[];
// The content scheme is specific to Android for identifying a stored file.
COMPONENT_EXPORT(URL) extern const char kContentScheme[];
COMPONENT_EXPORT(URL) extern const char kContentIDScheme[];
COMPONENT_EXPORT(URL) extern const char kDataScheme[];
COMPONENT_EXPORT(URL) extern const char kFileScheme[];
COMPONENT_EXPORT(URL) extern const char kFileSystemScheme[];
COMPONENT_EXPORT(URL) extern const char kFtpScheme[];
COMPONENT_EXPORT(URL) extern const char kHttpScheme[];
COMPONENT_EXPORT(URL) extern const char kHttpsScheme[];
COMPONENT_EXPORT(URL) extern const char kJavaScriptScheme[];
COMPONENT_EXPORT(URL) extern const char kMailToScheme[];
COMPONENT_EXPORT(URL) extern const char kQuicTransportScheme[];
COMPONENT_EXPORT(URL) extern const char kTelScheme[];
COMPONENT_EXPORT(URL) extern const char kWsScheme[];
COMPONENT_EXPORT(URL) extern const char kWssScheme[];

// Used to separate a standard scheme and the hostname: "://".
COMPONENT_EXPORT(URL) extern const char kStandardSchemeSeparator[];

COMPONENT_EXPORT(URL) extern const size_t kMaxURLChars;

}  // namespace url

#endif  // URL_URL_CONSTANTS_H_
