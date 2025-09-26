// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

int UnsafeIndex();  // This function might return an out-of-bound index.

// Expected rewrite:
// void FuncArg1Arr(base::span<int, 3> arg1) {
void FuncArg1Arr(base::span<int, 3> arg1) {
  arg1[UnsafeIndex()] = 3;
}

// Expected rewrite:
// void FuncArg2Arr(int arg1, base::span<int, 3> arg2) {
void FuncArg2Arr(int arg1, base::span<int, 3> arg2) {
  arg2[UnsafeIndex()] = 3;
}

// Expected rewrite:
// void FuncArg1Ptr(base::span<int> arg1) {
void FuncArg1Ptr(base::span<int> arg1) {
  arg1[UnsafeIndex()] = 3;
}

// Expected rewrite:
// void FuncArg2Ptr(int arg1, base::span<int> arg2) {
void FuncArg2Ptr(int arg1, base::span<int> arg2) {
  arg2[UnsafeIndex()] = 3;
}

// Expected rewrite:
// base::span<int> FuncRet() {
base::span<int> FuncRet() {
  static int arr[] = {1, 2, 3};
  return arr;
}

// Expected rewrite:
// base::span<int> FuncAll(base::span<int, 3> arg1,
//                         base::span<int> arg2,
//                         base::raw_span<int> arg3) {
base::span<int> FuncAll(base::span<int, 3> arg1,
                        base::span<int> arg2,
                        base::raw_span<int> arg3) {
  arg1[UnsafeIndex()] = 3;
  arg2[UnsafeIndex()] = 3;
  arg3[UnsafeIndex()] = 3;
  static int arr[] = {1, 2, 3};
  return arr;
}

void test_type_alias() {
  int arr[] = {1, 2, 3};

  // TODO(yukishiino): Currently the following line is needed to get FuncRet
  // spanified. This shouldn't be needed because we have other unsafe usages
  // through function pointers.
  FuncRet()[UnsafeIndex()] = 3;
  FuncAll(arr, arr, arr)[UnsafeIndex()] = 3;

  // Without a typedef / using declaration
  {
    // Expected rewrite:
    // void (*p_arg1arr_init)(base::span<int, 3> arg1) = FuncArg1Arr;
    void (*p_arg1arr_init)(base::span<int, 3> arg1) = FuncArg1Arr;
    p_arg1arr_init(arr);

    // Expected rewrite:
    // void (*p_arg2ptr_assign)(int arg1, base::span<int> arg2);
    void (*p_arg2ptr_assign)(int arg1, base::span<int> arg2);
    p_arg2ptr_assign = FuncArg2Ptr;
    p_arg2ptr_assign(3, arr);

    // Expected rewrite:
    // void (*p_arg2arr_init)(int, base::span<int, 3>) = FuncArg2Arr;
    void (*p_arg2arr_init)(int, base::span<int, 3>) = FuncArg2Arr;
    p_arg2arr_init(3, arr);

    // Expected rewrite:
    // base::span<int> (*p_ret_init)() = FuncRet;
    base::span<int> (*p_ret_init)() = FuncRet;
    // TODO(yukishiino): The following should be
    //     base::span<int> ret_init = ...
    int* ret_init = p_ret_init();
    ret_init[UnsafeIndex()] = 3;

    // Expected rewrite:
    // base::span<int> (*p_ret_assign)();
    base::span<int> (*p_ret_assign)();
    p_ret_assign = FuncRet;
    // TODO(yukishiino): The following should be
    //     base::span<int> ret_assign = ...
    int* ret_assign = p_ret_assign();
    ret_assign[UnsafeIndex()] = 3;
  }

  // With typedef declarations
  {
    // Expected rewrite:
    // typedef void (*Arg1ArrType)(base::span<int, 3> arg1);
    typedef void (*Arg1ArrType)(base::span<int, 3> arg1);
    Arg1ArrType p_arg1arr_init = FuncArg1Arr;
    p_arg1arr_init(arr);

    // Expected rewrite:
    // typedef void (*Arg2PtrType)(int arg1, base::span<int> arg2);
    typedef void (*Arg2PtrType)(int arg1, base::span<int> arg2);
    Arg2PtrType p_arg2ptr_assign;
    p_arg2ptr_assign = FuncArg2Ptr;
    p_arg2ptr_assign(3, arr);

    // Expected rewrite:
    // typedef void (*Arg2ArrType)(int, base::span<int, 3>);
    typedef void (*Arg2ArrType)(int, base::span<int, 3>);
    Arg2ArrType p_arg2arr_init = FuncArg2Arr;
    p_arg2arr_init(3, arr);

    // Expected rewrite:
    // typedef base::span<int> (*RetInitType)();
    typedef base::span<int> (*RetInitType)();
    RetInitType p_ret_init = FuncRet;
    // TODO(yukishiino): The following should be
    //     base::span<int> ret_init = ...
    int* ret_init = p_ret_init();
    ret_init[UnsafeIndex()] = 3;

    // Expected rewrite:
    // typedef base::span<int> (*RetAssignType)();
    typedef base::span<int> (*RetAssignType)();
    RetAssignType p_ret_assign;
    p_ret_assign = FuncRet;
    // TODO(yukishiino): The following should be
    //     base::span<int> ret_assign = ...
    int* ret_assign = p_ret_assign();
    ret_assign[UnsafeIndex()] = 3;
  }

  // With using declarations
  {
    // Expected rewrite:
    // using Arg1ArrType = void (*)(base::span<int, 3> arg1);
    using Arg1ArrType = void (*)(base::span<int, 3> arg1);
    Arg1ArrType p_arg1arr_init = FuncArg1Arr;
    p_arg1arr_init(arr);

    // Expected rewrite:
    // using Arg2PtrType = void (*)(int arg1, base::span<int> arg2);
    using Arg2PtrType = void (*)(int arg1, base::span<int> arg2);
    Arg2PtrType p_arg2ptr_assign;
    p_arg2ptr_assign = FuncArg2Ptr;
    p_arg2ptr_assign(3, arr);

    // Expected rewrite:
    // using Arg2ArrType = void (*)(int, base::span<int, 3>);
    using Arg2ArrType = void (*)(int, base::span<int, 3>);
    Arg2ArrType p_arg2arr_init = FuncArg2Arr;
    p_arg2arr_init(3, arr);

    // Expected rewrite:
    // using RetInitType = base::span<int> (*)();
    using RetInitType = base::span<int> (*)();
    RetInitType p_ret_init = FuncRet;
    // TODO(yukishiino): The following should be
    //     base::span<int> ret_init = ...
    int* ret_init = p_ret_init();
    ret_init[UnsafeIndex()] = 3;

    // Expected rewrite:
    // using RetAssignType = base::span<int> (*)();
    using RetAssignType = base::span<int> (*)();
    RetAssignType p_ret_assign;
    p_ret_assign = FuncRet;
    // TODO(yukishiino): The following should be
    //     base::span<int> ret_assign = ...
    int* ret_assign = p_ret_assign();
    ret_assign[UnsafeIndex()] = 3;
  }

  // With nested typedef/using and multiple rewritings on the func ptr type
  {
    // Expected rewrite:
    // typedef base::span<int> (*AllType1)(base::span<int, 3> arg1,
    //                                     base::span<int> arg2,
    //                                     base::raw_span<int> arg3);
    typedef base::span<int> (*AllType1)(base::span<int, 3> arg1,
                                        base::span<int> arg2,
                                        base::raw_span<int> arg3);
    using AllType2 = AllType1;
    AllType2 p_all = FuncAll;
    int* ret_all = p_all(arr, arr, arr);
    ret_all[UnsafeIndex()] = 3;
  }
}
