// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_H_
#define WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_H_

#include <string>


namespace weblayer {

struct WebMessage {
  WebMessage();
  ~WebMessage();

  std::u16string message;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_H_
