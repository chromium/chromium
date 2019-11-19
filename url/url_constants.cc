// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_constants.h"

namespace url {

const char kAboutBlankURL[] = "about:blank";
const char kAboutSrcdocURL[] = "about:srcdoc";

const char kAboutBlankPath[] = "blank";
const char kAboutSrcdocPath[] = "srcdoc";

const char kAboutScheme[] = "about";
const char kBlobScheme[] = "blob";
const char kContentScheme[] = "content";
const char kContentIDScheme[] = "cid";
const char kDataScheme[] = "data";
const char kFileScheme[] = "file";
const char kFileSystemScheme[] = "filesystem";
const char kFtpScheme[] = "ftp";
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kJavaScriptScheme[] = "javascript";
const char kMailToScheme[] = "mailto";
// This is for QuicTransport (https://wicg.github.io/web-transport/).
// See also: https://www.iana.org/assignments/uri-schemes/prov/quic-transport
const char kQuicTransportScheme[] = "quic-transport";
const char kTelScheme[] = "tel";
const char kWsScheme[] = "ws";
const char kWssScheme[] = "wss";

const char kStandardSchemeSeparator[] = "://";

const size_t kMaxURLChars = 2 * 1024 * 1024;

}  // namespace url
