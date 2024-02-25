// Copyright (c) Microsoft Corporation

#include "Edid.h"
#include <cassert>

namespace display::test {
bool EdidTimingEntry::SetMode(unsigned short width,
                              unsigned short height,
                              unsigned short freq) {
  if (width < 256 || width > 2288) {
    return false;
  }
  if (freq < 60 || freq > 123) {
    return false;
  }
  if (height == (width / 16) * 10) {
    aspect_ratio = 0b00;  // 16:10
  } else if (height == (width / 4) * 3) {
    aspect_ratio = 0b01;  // 4:3
  } else if (height == (width / 5) * 4) {
    aspect_ratio = 0b10;  // 5:4
  } else if (height == (width / 16) * 9) {
    aspect_ratio = 0b11;  // 16:9
  } else {
    return false;  // Invalid aspect ratio.
  }
  x_pixels = static_cast<unsigned char>((width / 8) - 31);
  vertical_frequency = static_cast<unsigned char>(freq - 60);
  return true;
}

int EdidTimingEntry::GetWidth() {
  return (x_pixels + 31) * 8;
}

int EdidTimingEntry::GetHeight() {
  switch (aspect_ratio) {
    default:
    case 0b00:
      return (GetWidth() / 16) * 10;  // 16:10 (EDID >= 1.3; 1:1 otherwise).
    case 0b01:
      return (GetWidth() / 4) * 3;  // 4:3
    case 0b10:
      return (GetWidth() / 5) * 4;  // 5:4
    case 0b11:
      return (GetWidth() / 16) * 9;  // 16:9
  }
}

int EdidTimingEntry::GetVerticalFrequency() {
  return vertical_frequency + 60;
}

EdidTimingEntry* Edid::GetTimingEntry(int entry) {
  assert(0 <= entry && entry < 8);
  // Timing entries start at byte 38.
  return reinterpret_cast<EdidTimingEntry*>(edidBlock.data() + 38 +
                                            (entry * sizeof(EdidTimingEntry)));
}

void Edid::SetProductCode(uint16_t code) {
  // Manufacturer product code is bytes 10-11.
  *(reinterpret_cast<uint16_t*>(edidBlock.data() + 10)) = code;
}

void Edid::SetSerialNumber(uint32_t serial) {
  // Serial number is bytes 12-15.
  *(reinterpret_cast<uint32_t*>(edidBlock.data() + 12)) = serial;
}

void Edid::UpdateChecksum() {
  assert(edidBlock.size() == kBlockSize);
  // Sum all bytes except the last.
  int sum = 0;
  for (int i = 0; i < edidBlock.size() - 1; i++) {
    sum += edidBlock.at(i);
  }
  // Set the last byte to make the sum of ALL bytes mod 256 equal 0.
  edidBlock[edidBlock.size() - 1] =
      (sum % 256) == 0 ? 0 : static_cast<unsigned char>(256 - (sum % 256));
  assert((sum + edidBlock[edidBlock.size() - 1]) % 256 == 0);
}

}  // namespace display::test
