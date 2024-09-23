// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "header-original.h"

// Declared from third_party.
void ProcessBuffer2(int* buffer, int size) {
  for (int i = 0; i < size; i++) {
    buffer[i] = buffer[i] * 2;
  }
}

// Declared from first party.
void ProcessBuffer3(int* buffer, int size) {
  for (int i = 0; i < size; i++) {
    buffer[i] = buffer[i] + 1;
  }
}
