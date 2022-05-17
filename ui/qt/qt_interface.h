// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_INTERFACE_H_
#define UI_QT_QT_INTERFACE_H_

// This file shouldn't include any standard C++ headers (directly or indirectly)

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
  char* c_str() { return str_; }

 private:
  char* str_ = nullptr;
};

enum class FontHinting {
  kDefault,
  kNone,
  kLight,
  kFull,
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

class QtInterface {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void FontChanged() = 0;
  };

  QtInterface() = default;
  QtInterface(const QtInterface&) = delete;
  QtInterface& operator=(const QtInterface&) = delete;
  virtual ~QtInterface() = default;

  virtual double GetScaleFactor() const = 0;
  virtual FontRenderParams GetFontRenderParams() const = 0;
  virtual FontDescription GetFontDescription() const = 0;
};

}  // namespace qt

// This should be the only thing exported from qt_shim.
extern "C" __attribute__((visibility("default"))) qt::QtInterface*
CreateQtInterface(qt::QtInterface::Delegate* delegate, int* argc, char** argv);

#endif  // UI_QT_QT_INTERFACE_H_
