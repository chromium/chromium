// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CONSTANTS_H_
#define URL_URL_CONSTANTS_H_

#include <stddef.h>

#include "base/component_export.h"

namespace url {

COMPONENT_EXPORT(URL) extern const char kAboutBlankURL[];
COMPONENT_EXPORT(URL) extern const char16_t kAboutBlankURL16[];
COMPONENT_EXPORT(URL) extern const char kAboutSrcdocURL[];
COMPONENT_EXPORT(URL) extern const char16_t kAboutSrcdocURL16[];

COMPONENT_EXPORT(URL) extern const char kAboutBlankPath[];
COMPONENT_EXPORT(URL) extern const char16_t kAboutBlankPath16[];
COMPONENT_EXPORT(URL) extern const char kAboutSrcdocPath[];
COMPONENT_EXPORT(URL) extern const char16_t kAboutSrcdocPath16[];

COMPONENT_EXPORT(URL) extern const char kAboutScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kAboutScheme16[];
COMPONENT_EXPORT(URL) extern const char kBlobScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kBlobScheme16[];
// The content scheme is specific to Android for identifying a stored file.
COMPONENT_EXPORT(URL) extern const char kContentScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kContentScheme16[];
COMPONENT_EXPORT(URL) extern const char kContentIDScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kContentIDScheme16[];
COMPONENT_EXPORT(URL) extern const char kDataScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kDataScheme16[];
COMPONENT_EXPORT(URL) extern const char kFileScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kFileScheme16[];
COMPONENT_EXPORT(URL) extern const char kFileSystemScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kFileSystemScheme16[];
COMPONENT_EXPORT(URL) extern const char kFtpScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kFtpScheme16[];
COMPONENT_EXPORT(URL) extern const char kHttpScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kHttpScheme16[];
COMPONENT_EXPORT(URL) extern const char kHttpsScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kHttpsScheme16[];
COMPONENT_EXPORT(URL) extern const char kJavaScriptScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kJavaScriptScheme16[];
COMPONENT_EXPORT(URL) extern const char kMailToScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kMailToScheme16[];
COMPONENT_EXPORT(URL) extern const char kQuicTransportScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kQuicTransportScheme16[];
COMPONENT_EXPORT(URL) extern const char kTelScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kTelScheme16[];
COMPONENT_EXPORT(URL) extern const char kUrnScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kUrnScheme16[];
COMPONENT_EXPORT(URL) extern const char kUuidInPackageScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kUuidInPackageScheme16[];
COMPONENT_EXPORT(URL) extern const char kWebcalScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kWebcalScheme16[];
COMPONENT_EXPORT(URL) extern const char kWsScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kWsScheme16[];
COMPONENT_EXPORT(URL) extern const char kWssScheme[];
COMPONENT_EXPORT(URL) extern const char16_t kWssScheme16[];

// Used to separate a standard scheme and the hostname: "://".
COMPONENT_EXPORT(URL) extern const char kStandardSchemeSeparator[];
COMPONENT_EXPORT(URL) extern const char16_t kStandardSchemeSeparator16[];

COMPONENT_EXPORT(URL) extern const size_t kMaxURLChars;

}  // namespace url

#endif  // URL_URL_CONSTANTS_H_
