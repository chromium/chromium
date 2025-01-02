// Copyright(C) 2022 Intel Corporation
// Licensed under the MIT License
#pragma once

struct OrtDnnlProviderOptions {
  int use_arena;          // If arena is used, use_arena 0 = not used, nonzero = used
  void* threadpool_args;  // Used to enable configure the oneDNN threadpool
};