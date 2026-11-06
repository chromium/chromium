// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/util/display_manager_test_util.h"

#include <stdint.h>

#include "base/check_op.h"

namespace display {

namespace {
// Use larger than max int to catch overflow early.
constexpr int64_t kSynthesizedDisplayIdStart = 2200000000LL;

int64_t next_synthesized_display_id = kSynthesizedDisplayIdStart;

}  // namespace

void ResetDisplayIdForTest() {
  next_synthesized_display_id = kSynthesizedDisplayIdStart;
}

int64_t GetASynthesizedDisplayId() {
  const int64_t id = next_synthesized_display_id;
  next_synthesized_display_id =
      SynthesizeDisplayIdFromSeed(next_synthesized_display_id);
  return id;
}

int64_t SynthesizeDisplayIdFromSeed(int64_t id) {
  // Connector index is stored in the first 8 bits for port-base display IDs.
  int next_output_index = id & 0xFF;
  next_output_index++;
  DCHECK_GT(0x100, next_output_index);
  const int64_t base = static_cast<int64_t>(~static_cast<uint64_t>(0xFF) & id);

  if (id == kSynthesizedDisplayIdStart) {
    return id + 0x100 + next_output_index;
  }
  return base + next_output_index;
}

int64_t ProduceAlternativeSchemeIdForId(int64_t id) {
  DCHECK_GT(id, 0);
  int magnitude = 1;
  int64_t id_copy = id;
  do {
    magnitude *= 10;
    id_copy /= 10;
  } while (id_copy / 10);

  const int64_t new_id = id - magnitude;
  return new_id > 0 ? new_id : id + magnitude;
}

}  // namespace display
