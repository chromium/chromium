// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_
#define WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_

#include <string>

class GURL;

namespace weblayer {

// An interface that allows clients to handle download requests originating in
// the browser.
class DownloadDelegate {
 public:
  // A download of |url| has been requested with the specified details. If
  // ignored, the download will be dropped.
  virtual void DownloadRequested(const GURL& url,
                                 const std::string& user_agent,
                                 const std::string& content_disposition,
                                 const std::string& mime_type,
                                 int64_t content_length) = 0;

 protected:
  virtual ~DownloadDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_
