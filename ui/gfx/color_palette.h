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
constexpr SkColor kGoogleBlue050 = SkColorSetRGB(0xE8, 0xF0, 0xFE);
constexpr SkColor kGoogleBlue100 = SkColorSetRGB(0xD2, 0xE3, 0xFC);
constexpr SkColor kGoogleBlue200 = SkColorSetRGB(0xAE, 0xCB, 0xFA);
constexpr SkColor kGoogleBlue300 = SkColorSetRGB(0x8A, 0xB4, 0xF8);
constexpr SkColor kGoogleBlue400 = SkColorSetRGB(0x66, 0x9D, 0xF6);
constexpr SkColor kGoogleBlue500 = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr SkColor kGoogleBlue600 = SkColorSetRGB(0x1A, 0x73, 0xE8);
constexpr SkColor kGoogleBlue700 = SkColorSetRGB(0x19, 0x67, 0xD2);
constexpr SkColor kGoogleBlue800 = SkColorSetRGB(0x18, 0x5A, 0xBC);
constexpr SkColor kGoogleBlue900 = SkColorSetRGB(0x17, 0x4E, 0xA6);

constexpr SkColor kGoogleBlueDark400 = SkColorSetRGB(0x6B, 0xA5, 0xED);
constexpr SkColor kGoogleBlueDark600 = SkColorSetRGB(0x25, 0x81, 0xDF);

constexpr SkColor kGoogleRed050 = SkColorSetRGB(0xFC, 0x8E, 0xE6);
constexpr SkColor kGoogleRed100 = SkColorSetRGB(0xFA, 0xD2, 0xCF);
constexpr SkColor kGoogleRed200 = SkColorSetRGB(0xF6, 0xAE, 0xA9);
constexpr SkColor kGoogleRed300 = SkColorSetRGB(0xF2, 0x8B, 0x82);
constexpr SkColor kGoogleRed400 = SkColorSetRGB(0xEE, 0x67, 0x5C);
constexpr SkColor kGoogleRed500 = SkColorSetRGB(0xEA, 0x43, 0x35);
constexpr SkColor kGoogleRed600 = SkColorSetRGB(0xD9, 0x30, 0x25);
constexpr SkColor kGoogleRed700 = SkColorSetRGB(0xC5, 0x22, 0x1F);
constexpr SkColor kGoogleRed800 = SkColorSetRGB(0xB3, 0x14, 0x12);
constexpr SkColor kGoogleRed900 = SkColorSetRGB(0xA5, 0x0E, 0x0E);

constexpr SkColor kGoogleRedDark500 = SkColorSetRGB(0xE6, 0x6A, 0x5E);
constexpr SkColor kGoogleRedDark600 = SkColorSetRGB(0xD3, 0x3B, 0x30);
constexpr SkColor kGoogleRedDark800 = SkColorSetRGB(0xB4, 0x1B, 0x1A);

constexpr SkColor kGoogleGreen050 = SkColorSetRGB(0xE6, 0xF4, 0xEA);
constexpr SkColor kGoogleGreen100 = SkColorSetRGB(0xCE, 0xEA, 0xD6);
constexpr SkColor kGoogleGreen200 = SkColorSetRGB(0xA8, 0xDA, 0xB5);
constexpr SkColor kGoogleGreen300 = SkColorSetRGB(0x81, 0xC9, 0x95);
constexpr SkColor kGoogleGreen400 = SkColorSetRGB(0x5B, 0xB9, 0x74);
constexpr SkColor kGoogleGreen500 = SkColorSetRGB(0x34, 0xA8, 0x53);
constexpr SkColor kGoogleGreen600 = SkColorSetRGB(0x1E, 0x8E, 0x3E);
constexpr SkColor kGoogleGreen700 = SkColorSetRGB(0x18, 0x80, 0x38);
constexpr SkColor kGoogleGreen800 = SkColorSetRGB(0x13, 0x73, 0x33);
constexpr SkColor kGoogleGreen900 = SkColorSetRGB(0x0D, 0x65, 0x2D);

constexpr SkColor kGoogleGreenDark500 = SkColorSetRGB(0x41, 0xAF, 0x6A);
constexpr SkColor kGoogleGreenDark600 = SkColorSetRGB(0x28, 0x99, 0x4F);

constexpr SkColor kGoogleYellow050 = SkColorSetRGB(0xFE, 0xF7, 0xE0);
constexpr SkColor kGoogleYellow100 = SkColorSetRGB(0xFE, 0xEF, 0xC3);
constexpr SkColor kGoogleYellow200 = SkColorSetRGB(0xFB, 0xBC, 0x04);
constexpr SkColor kGoogleYellow300 = SkColorSetRGB(0xFD, 0xD6, 0x63);
constexpr SkColor kGoogleYellow400 = SkColorSetRGB(0xFC, 0xC9, 0x34);
constexpr SkColor kGoogleYellow500 = SkColorSetRGB(0xFB, 0xBC, 0x04);
constexpr SkColor kGoogleYellow600 = SkColorSetRGB(0xF9, 0xAB, 0x00);
constexpr SkColor kGoogleYellow700 = SkColorSetRGB(0xF2, 0x99, 0x00);
constexpr SkColor kGoogleYellow800 = SkColorSetRGB(0xEA, 0x86, 0x00);
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

constexpr SkColor kGoogleOrange050 = SkColorSetRGB(0xFE, 0xEF, 0xE3);
constexpr SkColor kGoogleOrange100 = SkColorSetRGB(0xFE, 0xDF, 0xC8);
constexpr SkColor kGoogleOrange200 = SkColorSetRGB(0xFD, 0xC6, 0x9C);
constexpr SkColor kGoogleOrange300 = SkColorSetRGB(0xFC, 0xAD, 0x70);
constexpr SkColor kGoogleOrange400 = SkColorSetRGB(0xFA, 0x90, 0x3E);
constexpr SkColor kGoogleOrange500 = SkColorSetRGB(0xFA, 0x7B, 0x17);
constexpr SkColor kGoogleOrange600 = SkColorSetRGB(0xE8, 0x71, 0x0A);
constexpr SkColor kGoogleOrange700 = SkColorSetRGB(0xD5, 0x6E, 0x0C);
constexpr SkColor kGoogleOrange800 = SkColorSetRGB(0xC2, 0x64, 0x01);
constexpr SkColor kGoogleOrange900 = SkColorSetRGB(0xB0, 0x60, 0x00);

constexpr SkColor kGooglePink050 = SkColorSetRGB(0xFD, 0xE7, 0xF3);
constexpr SkColor kGooglePink100 = SkColorSetRGB(0xFD, 0xCF, 0xE8);
constexpr SkColor kGooglePink200 = SkColorSetRGB(0xFB, 0xA9, 0xD6);
constexpr SkColor kGooglePink300 = SkColorSetRGB(0xFF, 0x8B, 0xCB);
constexpr SkColor kGooglePink400 = SkColorSetRGB(0xFF, 0x63, 0xB8);
constexpr SkColor kGooglePink500 = SkColorSetRGB(0xF4, 0x39, 0xA0);
constexpr SkColor kGooglePink600 = SkColorSetRGB(0xE5, 0x25, 0x92);
constexpr SkColor kGooglePink700 = SkColorSetRGB(0xD0, 0x18, 0x84);
constexpr SkColor kGooglePink800 = SkColorSetRGB(0xB8, 0x06, 0x72);
constexpr SkColor kGooglePink900 = SkColorSetRGB(0x9C, 0x16, 0x6B);

constexpr SkColor kGooglePurple050 = SkColorSetRGB(0xF3, 0xE8, 0xFD);
constexpr SkColor kGooglePurple100 = SkColorSetRGB(0xE9, 0xD2, 0xFD);
constexpr SkColor kGooglePurple200 = SkColorSetRGB(0xD7, 0xAE, 0xFB);
constexpr SkColor kGooglePurple300 = SkColorSetRGB(0xC5, 0x8A, 0xF9);
constexpr SkColor kGooglePurple400 = SkColorSetRGB(0xAF, 0x5C, 0xF7);
constexpr SkColor kGooglePurple500 = SkColorSetRGB(0xA1, 0x42, 0xF4);
constexpr SkColor kGooglePurple600 = SkColorSetRGB(0x93, 0x34, 0xE6);
constexpr SkColor kGooglePurple700 = SkColorSetRGB(0x84, 0x30, 0xCE);
constexpr SkColor kGooglePurple800 = SkColorSetRGB(0x76, 0x27, 0xBB);
constexpr SkColor kGooglePurple900 = SkColorSetRGB(0x68, 0x1D, 0xA8);

constexpr SkColor kGoogleCyan050 = SkColorSetRGB(0xE4, 0xF7, 0xFB);
constexpr SkColor kGoogleCyan100 = SkColorSetRGB(0xCB, 0xF0, 0xF8);
constexpr SkColor kGoogleCyan200 = SkColorSetRGB(0xA1, 0xE4, 0xF2);
constexpr SkColor kGoogleCyan300 = SkColorSetRGB(0x78, 0xD9, 0xEC);
constexpr SkColor kGoogleCyan400 = SkColorSetRGB(0x4E, 0xCD, 0xE6);
constexpr SkColor kGoogleCyan500 = SkColorSetRGB(0x24, 0xC1, 0xE0);
constexpr SkColor kGoogleCyan600 = SkColorSetRGB(0x12, 0xB5, 0xCB);
constexpr SkColor kGoogleCyan700 = SkColorSetRGB(0x12, 0x9E, 0xAF);
constexpr SkColor kGoogleCyan800 = SkColorSetRGB(0x09, 0x85, 0x91);
constexpr SkColor kGoogleCyan900 = SkColorSetRGB(0x00, 0x7B, 0x83);

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
