// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_UTIL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_UTIL_H_

#include <limits.h>

namespace ui {

#define EVDEV_LONG_BITS (CHAR_BIT * sizeof(long))
#define EVDEV_INT64_BITS (CHAR_BIT * sizeof(int64_t))
#define EVDEV_BITS_TO_LONGS(x) (((x) + EVDEV_LONG_BITS - 1) / EVDEV_LONG_BITS)
#define EVDEV_BITS_TO_INT64(x) (((x) + EVDEV_INT64_BITS - 1) / EVDEV_INT64_BITS)

static inline bool EvdevBitIsSet(const unsigned long* data, int bit) {
  return data[bit / EVDEV_LONG_BITS] & (1UL << (bit % EVDEV_LONG_BITS));
}

static inline bool EvdevBitUint64IsSet(const uint64_t* data, int bit) {
  return data[bit / EVDEV_INT64_BITS] &
         ((uint64_t)1 << (bit % EVDEV_INT64_BITS));
}

static inline void EvdevSetBit(unsigned long* data, int bit) {
  data[bit / EVDEV_LONG_BITS] |= (1UL << (bit % EVDEV_LONG_BITS));
}

static inline void EvdevSetUint64Bit(uint64_t* data, int bit) {
  data[bit / EVDEV_INT64_BITS] |= ((uint64_t)1 << (bit % EVDEV_INT64_BITS));
}

static inline void EvdevClearBit(unsigned long* data, int bit) {
  data[bit / EVDEV_LONG_BITS] &= ~(1UL << (bit % EVDEV_LONG_BITS));
}

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_UTIL_H_
