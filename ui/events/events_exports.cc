// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for including headers that are not included in any other .cc
// files contained with the ui/events module.  We need to include these here so
// that linker will know to include the symbols, defined by these headers, in
// the resulting dynamic library.

#include "ui/events/event_observer.h"
