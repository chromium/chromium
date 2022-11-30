// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOZILLA_MOZILLAEXPORT_H_
#define THIRD_PARTY_MOZILLA_MOZILLAEXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(MOZILLA_IMPLEMENTATION)
#define MOZILLA_EXPORT __attribute__((visibility("default")))
#else
#define MOZILLA_EXPORT
#endif

#else  // !defined(COMPONENT_BUILD)

#define MOZILLA_EXPORT

#endif

#endif  // THIRD_PARTY_MOZILLA_MOZILLAEXPORT_H_
