// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_INTERFACE_H_
#define UI_QT_QT_INTERFACE_H_

// This file shouldn't include any standard C++ headers (directly or indirectly)

#if defined(__has_attribute) && __has_attribute(no_sanitize)
#define DISABLE_CFI_VCALL __attribute__((no_sanitize("cfi-vcall")))
#else
#define DISABLE_CFI_VCALL
#endif

#include <stdint.h>
#include <stdlib.h>

using SkColor = uint32_t;

namespace qt {

// std::string cannot be passed over the library boundary, so this class acts
// as an interface between QT and Chrome.
class String {
 public:
  String();
  explicit String(const char* str);
  String(String&& other);
  String& operator=(String&& other);
  ~String();

  // May be nullptr.
  const char* c_str() const { return str_; }

 private:
  char* str_ = nullptr;
};

// A generic bag of bytes.
class Buffer {
 public:
  Buffer();
  // Creates a copy of `data`.
  Buffer(const uint8_t* data, size_t size);
  Buffer(Buffer&& other);
  Buffer& operator=(Buffer&& other);
  ~Buffer();

  // Take ownership of the data in this buffer (resetting `this`).
  uint8_t* Take();

  uint8_t* data() { return data_; }
  size_t size() const { return size_; }

 private:
  uint8_t* data_ = nullptr;
  size_t size_ = 0;
};

enum class FontHinting {
  kDefault,
  kNone,
  kLight,
  kFull,
};

enum class ColorType {
  kWindowBg,
  kWindowFg,
  kHighlightBg,
  kHighlightFg,
  kEntryBg,
  kEntryFg,
  kButtonBg,
  kButtonFg,

  kLight,
  kMidlight,
  kDark,
  kMidground,
  kShadow,
};

enum class ColorState {
  kNormal,
  kDisabled,
  kInactive,
};

struct FontRenderParams {
  bool antialiasing;
  bool use_bitmaps;
  FontHinting hinting;
};

struct FontDescription {
  String family;
  int size_pixels;
  int size_points;
  bool is_italic;
  int weight;
};

struct Image {
  int width = 0;
  int height = 0;
  float scale = 1.0f;
  // The data is stored as ARGB32 (premultiplied).
  Buffer data_argb;
};

struct MonitorScale {
  int x_px;
  int y_px;
  int width_px;
  int height_px;
  float scale;
};

class QtInterface {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void FontChanged() = 0;
    virtual void ThemeChanged() = 0;
    virtual void ScaleFactorMaybeChanged() = 0;
  };

  QtInterface() = default;
  QtInterface(const QtInterface&) = delete;
  QtInterface& operator=(const QtInterface&) = delete;
  virtual ~QtInterface() = default;

  // Returns the size of `monitors`.  `monitors` will be valid while this class
  // is alive until the next call to `GetMonitorConfig()`.
  virtual size_t GetMonitorConfig(MonitorScale** monitors,
                                  float* primary_scale) = 0;
  virtual FontRenderParams GetFontRenderParams() const = 0;
  virtual FontDescription GetFontDescription() const = 0;
  virtual Image GetIconForContentType(const String& content_type,
                                      int size) const = 0;
  virtual SkColor GetColor(ColorType role, ColorState state) const = 0;
  virtual SkColor GetFrameColor(ColorState state,
                                bool use_custom_frame) const = 0;
  virtual Image DrawHeader(int width,
                           int height,
                           SkColor default_color,
                           ColorState state,
                           bool use_custom_frame) const = 0;
  virtual int GetCursorBlinkIntervalMs() const = 0;
  virtual int GetAnimationDurationMs() const = 0;
};

}  // namespace qt

// This should be the only thing exported from qt_shim.
extern "C" __attribute__((visibility("default"))) qt::QtInterface*
CreateQtInterface(qt::QtInterface::Delegate* delegate, int* argc, char** argv);

#endif  // UI_QT_QT_INTERFACE_H_
