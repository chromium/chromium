// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

enum ResourceOffset {
  cpu_resource_offset = 0,
  cuda_resource_offset = 10000,
  dml_resource_offset = 20000,
  rocm_resource_offset = 30000,
  // offsets for other ort eps
  custom_ep_resource_offset = 10000000,
  // offsets for customized eps
};