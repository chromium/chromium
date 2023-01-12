// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_
#define WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace weblayer {
class Download;
class Tab;

using AllowDownloadCallback = base::OnceCallback<void(bool /*allow*/)>;

// An interface that allows clients to handle download requests originating in
// the browser. The object is safe to hold on to until DownloadCompleted or
// DownloadFailed are called.
class DownloadDelegate {
 public:
  // Gives the embedder the opportunity to asynchronously allow or disallow the
  // given download. The download is paused until the callback is run. It's safe
  // to run |callback| synchronously.
  virtual void AllowDownload(Tab* tab,
                             const GURL& url,
                             const std::string& request_method,
                             absl::optional<url::Origin> request_initiator,
                             AllowDownloadCallback callback) = 0;

  // A download of |url| has been requested with the specified details. If
  // it returns true the download will be considered intercepted and WebLayer
  // won't proceed with it. Note that there are many corner cases where the
  // embedder downloading it won't work (e.g. POSTs, one-time URLs, requests
  // that depend on cookies or auth state). This is called after AllowDownload.
  virtual bool InterceptDownload(const GURL& url,
                                 const std::string& user_agent,
                                 const std::string& content_disposition,
                                 const std::string& mime_type,
                                 int64_t content_length) = 0;

  // A download has started. There will be 0..n calls to
  // DownloadProgressChanged, then either a call to DownloadCompleted or
  // DownloadFailed.
  virtual void DownloadStarted(Download* download) {}

  // The progress percentage of a download has changed.
  virtual void DownloadProgressChanged(Download* download) {}

  // A download has completed successfully.
  virtual void DownloadCompleted(Download* download) {}

  // A download has failed because the user cancelled it or because of a server
  // or network error.
  virtual void DownloadFailed(Download* download) {}

 protected:
  virtual ~DownloadDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_
