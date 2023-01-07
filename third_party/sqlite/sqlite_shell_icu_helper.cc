// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/sqlite/sqlite_shell_icu_helper.h"

#include "base/check.h"
#include "base/i18n/icu_util.h"

void InitializeICUForSqliteShell() {
  CHECK(base::i18n::InitializeICU());
}