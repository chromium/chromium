// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERSISTENCE_MINIMAL_BROWSER_PERSISTER_H_
#define WEBLAYER_BROWSER_PERSISTENCE_MINIMAL_BROWSER_PERSISTER_H_

#include <stddef.h>

#include <vector>

namespace weblayer {

class BrowserImpl;

// Returns a byte array that can later be used to restore the state (Tabs and
// navigations) of a Browser. This does not store the full state, only a
// minimal state. For example, it may not include all tabs or all navigations.
// |max_size_in_bytes| is provided for tests and allows specifying the max.
// A value of 0 means use the default max.
std::vector<uint8_t> PersistMinimalState(BrowserImpl* browser,
                                         int max_size_in_bytes = 0);

// Restores the state previously created via PersistMinimalState(). When
// done this ensures |browser| has at least one tab.
void RestoreMinimalState(BrowserImpl* browser,
                         const std::vector<uint8_t>& value);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERSISTENCE_MINIMAL_BROWSER_PERSISTER_H_
