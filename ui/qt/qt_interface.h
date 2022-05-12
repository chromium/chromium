// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_INTERFACE_H_
#define UI_QT_QT_INTERFACE_H_

// This file shouldn't include any standard C++ headers (directly or indirectly)

namespace qt {

class QtInterface {
 public:
  QtInterface() = default;
  QtInterface(const QtInterface&) = delete;
  QtInterface& operator=(const QtInterface&) = delete;
  virtual ~QtInterface() = default;

  virtual double GetScaleFactor() const = 0;
};

}  // namespace qt

// This should be the only thing exported from qt_shim.
extern "C" __attribute__((visibility("default"))) qt::QtInterface*
CreateQtInterface(int* argc, char** argv);

#endif  // UI_QT_QT_INTERFACE_H_
