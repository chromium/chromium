// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_ACCELERATION_CONFIGURATION_C_DELEGATE_PLUGIN_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_ACCELERATION_CONFIGURATION_C_DELEGATE_PLUGIN_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/litert/src/tflite/acceleration/configuration/c/delegate_plugin.h"
#else
#include_next "tensorflow/lite/acceleration/configuration/c/delegate_plugin.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_ACCELERATION_CONFIGURATION_C_DELEGATE_PLUGIN_H_
