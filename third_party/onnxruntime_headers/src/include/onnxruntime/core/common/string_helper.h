// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <string>

// forward declaration
struct OrtAllocator;
namespace onnxruntime {
char* StrDup(const std::string& str, OrtAllocator* allocator);
}  // namespace onnxruntime
