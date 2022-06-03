// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_RESOURCE_MAPPER_H_
#define WEBLAYER_BROWSER_ANDROID_RESOURCE_MAPPER_H_

namespace weblayer {

// Converts the given chromium |resource_id| (e.g. IDR_INFOBAR_TRANSLATE) to
// an Android drawable resource ID. Returns 0 if a mapping wasn't found.
int MapToJavaDrawableId(int resource_id);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_RESOURCE_MAPPER_H_
