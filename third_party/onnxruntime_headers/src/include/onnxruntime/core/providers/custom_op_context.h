// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

// CustomOpContext defines an interface allowing a custom op to access ep-specific resources.
struct CustomOpContext {
  CustomOpContext() = default;
  virtual ~CustomOpContext() {};
};