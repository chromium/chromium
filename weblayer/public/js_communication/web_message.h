// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_H_
#define WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_H_

#include <string>

#include "base/strings/string16.h"

namespace weblayer {

struct WebMessage {
  WebMessage();
  ~WebMessage();

  base::string16 message;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_H_
