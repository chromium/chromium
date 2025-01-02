// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <optional>

namespace onnxruntime {

using std::optional;

#ifndef ORT_NO_EXCEPTIONS
using std::bad_optional_access;
#endif

using std::nullopt;
using std::nullopt_t;

using std::in_place;
using std::in_place_t;

using std::make_optional;

}  // namespace onnxruntime
