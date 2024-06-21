// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CONSTANTS_H_
#define URL_URL_CONSTANTS_H_

#include <stddef.h>

namespace url {

inline constexpr char kAboutBlankURL[] = "about:blank";
inline constexpr char16_t kAboutBlankURL16[] = u"about:blank";
inline constexpr char kAboutSrcdocURL[] = "about:srcdoc";
inline constexpr char16_t kAboutSrcdocURL16[] = u"about:srcdoc";

inline constexpr char kAboutBlankPath[] = "blank";
inline constexpr char16_t kAboutBlankPath16[] = u"blank";
inline constexpr char kAboutSrcdocPath[] = "srcdoc";
inline constexpr char16_t kAboutSrcdocPath16[] = u"srcdoc";

inline constexpr char kAboutScheme[] = "about";
inline constexpr char16_t kAboutScheme16[] = u"about";
inline constexpr char kAndroidScheme[] = "android";
inline constexpr char kBlobScheme[] = "blob";
inline constexpr char16_t kBlobScheme16[] = u"blob";
inline constexpr char kChromeosSteamScheme[] = "chromeos-steam";
inline constexpr char kContentScheme[] = "content";
inline constexpr char16_t kContentScheme16[] = u"content";
inline constexpr char kContentIDScheme[] = "cid";
inline constexpr char16_t kContentIDScheme16[] = u"cid";
inline constexpr char kDataScheme[] = "data";
inline constexpr char16_t kDataScheme16[] = u"data";
inline constexpr char kDrivefsScheme[] = "drivefs";
inline constexpr char kFileScheme[] = "file";
inline constexpr char16_t kFileScheme16[] = u"file";
inline constexpr char kFileSystemScheme[] = "filesystem";
inline constexpr char16_t kFileSystemScheme16[] = u"filesystem";
inline constexpr char kFtpScheme[] = "ftp";
inline constexpr char16_t kFtpScheme16[] = u"ftp";
inline constexpr char kHttpScheme[] = "http";
inline constexpr char16_t kHttpScheme16[] = u"http";
inline constexpr char kHttpsScheme[] = "https";
inline constexpr char16_t kHttpsScheme16[] = u"https";
inline constexpr char kJavaScriptScheme[] = "javascript";
inline constexpr char16_t kJavaScriptScheme16[] = u"javascript";
inline constexpr char kMailToScheme[] = "mailto";
inline constexpr char16_t kMailToScheme16[] = u"mailto";
inline constexpr char kMaterializedViewScheme[] = "materialized-view";
inline constexpr char kSteamScheme[] = "steam";
inline constexpr char kTelScheme[] = "tel";
inline constexpr char16_t kTelScheme16[] = u"tel";
inline constexpr char kUrnScheme[] = "urn";
inline constexpr char16_t kUrnScheme16[] = u"urn";
inline constexpr char kUuidInPackageScheme[] = "uuid-in-package";
inline constexpr char16_t kUuidInPackageScheme16[] = u"uuid-in-package";
inline constexpr char kWebcalScheme[] = "webcal";
inline constexpr char16_t kWebcalScheme16[] = u"webcal";
inline constexpr char kWsScheme[] = "ws";
inline constexpr char16_t kWsScheme16[] = u"ws";
inline constexpr char kWssScheme[] = "wss";
inline constexpr char16_t kWssScheme16[] = u"wss";

// Used to separate a standard scheme and the hostname: "://".
inline constexpr char kStandardSchemeSeparator[] = "://";
inline constexpr char16_t kStandardSchemeSeparator16[] = u"://";

// Max GURL length passed between processes. See url::mojom::kMaxURLChars, which
// has the same value, for more details.
inline constexpr size_t kMaxURLChars = 2 * 1024 * 1024;

}  // namespace url

#endif  // URL_URL_CONSTANTS_H_
