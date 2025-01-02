// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "core/session/onnxruntime_c_api.h"

struct OrtCustomOpDomain {
  std::string domain_;
  std::vector<const OrtCustomOp*> custom_ops_;
};
