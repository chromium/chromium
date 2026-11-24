// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define IDENTITY(x) x
#define FORWARD(x) IDENTITY(x)
#define VALUE() 0
#define DECLARE_ENUM(name, value) \
  enum class name { kOne = 1, kMaxValue = value }

#include <filtered_ast_consumer.h>
