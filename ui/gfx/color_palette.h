// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_PALETTE_H_
#define UI_GFX_COLOR_PALETTE_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {

// A placeholder value for unset colors. This should never be visible and is red
// as a visual flag for misbehaving code.
constexpr SkColor kPlaceholderColor = SK_ColorRED;

// The number refers to the shade of darkness. Each color in the MD
// palette ranges from 050-900.
constexpr SkColor kGoogleBlue300 = SkColorSetRGB(0x8A, 0xB4, 0xF8);
constexpr SkColor kGoogleBlue500 = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr SkColor kGoogleBlue600 = SkColorSetRGB(0x1A, 0x73, 0xE8);
constexpr SkColor kGoogleBlue700 = SkColorSetRGB(0x19, 0x67, 0xD2);
constexpr SkColor kGoogleBlue900 = SkColorSetRGB(0x17, 0x4E, 0xA6);
constexpr SkColor kGoogleBlueDark400 = SkColorSetRGB(0x6B, 0xA5, 0xED);
constexpr SkColor kGoogleBlueDark600 = SkColorSetRGB(0x25, 0x81, 0xDF);
constexpr SkColor kGoogleRed300 = SkColorSetRGB(0xF2, 0x8B, 0xB2);
constexpr SkColor kGoogleRed500 = SkColorSetRGB(0xEA, 0x43, 0x35);
constexpr SkColor kGoogleRed600 = SkColorSetRGB(0xD9, 0x30, 0x25);
constexpr SkColor kGoogleRed700 = SkColorSetRGB(0xC5, 0x22, 0x1F);
constexpr SkColor kGoogleRed800 = SkColorSetRGB(0xB3, 0x14, 0x12);
constexpr SkColor kGoogleRedDark500 = SkColorSetRGB(0xE6, 0x6A, 0x5E);
constexpr SkColor kGoogleRedDark600 = SkColorSetRGB(0xD3, 0x3B, 0x30);
constexpr SkColor kGoogleRedDark800 = SkColorSetRGB(0xB4, 0x1B, 0x1A);
constexpr SkColor kGoogleGreen300 = SkColorSetRGB(0x81, 0xC9, 0x95);
constexpr SkColor kGoogleGreen600 = SkColorSetRGB(0x1E, 0x8E, 0x3E);
constexpr SkColor kGoogleGreen700 = SkColorSetRGB(0x18, 0x80, 0x38);
constexpr SkColor kGoogleGreenDark500 = SkColorSetRGB(0x41, 0xAF, 0x6A);
constexpr SkColor kGoogleGreenDark600 = SkColorSetRGB(0x28, 0x99, 0x4F);
constexpr SkColor kGoogleYellow300 = SkColorSetRGB(0xFD, 0xD6, 0x63);
constexpr SkColor kGoogleYellow700 = SkColorSetRGB(0xF2, 0x99, 0x00);
constexpr SkColor kGoogleYellow900 = SkColorSetRGB(0xE3, 0x74, 0x00);
constexpr SkColor kGoogleGrey050 = SkColorSetRGB(0xF8, 0xF9, 0xFA);
constexpr SkColor kGoogleGrey100 = SkColorSetRGB(0xF1, 0xF3, 0xF4);
constexpr SkColor kGoogleGrey200 = SkColorSetRGB(0xE8, 0xEA, 0xED);
constexpr SkColor kGoogleGrey300 = SkColorSetRGB(0xDA, 0xDC, 0xE0);
constexpr SkColor kGoogleGrey400 = SkColorSetRGB(0xBD, 0xC1, 0xC6);
constexpr SkColor kGoogleGrey500 = SkColorSetRGB(0x9A, 0xA0, 0xA6);
constexpr SkColor kGoogleGrey600 = SkColorSetRGB(0x80, 0x86, 0x8B);
constexpr SkColor kGoogleGrey700 = SkColorSetRGB(0x5F, 0x63, 0x68);
constexpr SkColor kGoogleGrey800 = SkColorSetRGB(0x3C, 0x40, 0x43);
constexpr SkColor kGoogleGrey900 = SkColorSetRGB(0x20, 0x21, 0x24);

// The following are the values that correspond to the above kGoogleGreyXXX
// values, which are the opaque colors created from the following alpha values
// applied to kGoogleGrey900 on a white background.
// These values are from the palette of greys specified in the Material Refresh
// spec.
constexpr SkAlpha kGoogleGreyAlpha050 = 0x08;  //   3%
constexpr SkAlpha kGoogleGreyAlpha100 = 0x0F;  //   6%
constexpr SkAlpha kGoogleGreyAlpha200 = 0x1A;  //  10%
constexpr SkAlpha kGoogleGreyAlpha300 = 0x29;  //  16%
constexpr SkAlpha kGoogleGreyAlpha400 = 0x47;  //  28%
constexpr SkAlpha kGoogleGreyAlpha500 = 0x6E;  //  43%
constexpr SkAlpha kGoogleGreyAlpha600 = 0x8C;  //  55%
constexpr SkAlpha kGoogleGreyAlpha700 = 0xB5;  //  71%
constexpr SkAlpha kGoogleGreyAlpha800 = 0xDB;  //  86%

// kChromeIconGrey is subject to change in the future, kGoogleGrey700 is set in
// stone. If you're semantically looking for "the icon color Chrome uses" then
// use kChromeIconGrey, if you're looking for GG700 grey specifically, use the
// Google-grey constant directly.
constexpr SkColor kChromeIconGrey = kGoogleGrey700;

// An alpha value for designating a control's disabled state. In specs this is
// sometimes listed as 0.38a.
constexpr SkAlpha kDisabledControlAlpha = 0x61;

}  // namespace gfx

#endif  // UI_GFX_COLOR_PALETTE_H_
