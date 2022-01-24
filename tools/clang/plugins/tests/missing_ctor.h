// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MISSING_CTOR_H_
#define MISSING_CTOR_H_

#include <string>
#include "base/memory/checked_ptr.h"
#include "base/memory/raw_ptr.h"

struct MyString {
  MyString();
  ~MyString();
  MyString(const MyString&);
  MyString(MyString&&);
};

template <class T>
struct MyVector {
  MyVector();
  ~MyVector();
  MyVector(const MyVector&);
  MyVector(MyVector&&);
};

// Note: this should warn for an implicit copy constructor too, but currently
// doesn't, due to a plugin bug.
class MissingCtorsArentOKInHeader {
 public:

 private:
  MyVector<int> one_;
  MyVector<MyString> two_;
};

// Inline move ctors shouldn't be warned about. Similar to the previous test
// case, this also incorrectly fails to warn for the implicit copy ctor.
class InlineImplicitMoveCtorOK {
 public:
  InlineImplicitMoveCtorOK();

 private:
  // ctor weight = 12, dtor weight = 9.
  MyString one_;
  MyString two_;
  MyString three_;
  int four_;
  int five_;
  int six_;
};

class ExplicitlyDefaultedInlineAlsoWarns {
 public:
  ExplicitlyDefaultedInlineAlsoWarns() = default;
  ~ExplicitlyDefaultedInlineAlsoWarns() = default;
  ExplicitlyDefaultedInlineAlsoWarns(
      const ExplicitlyDefaultedInlineAlsoWarns&) = default;

 private:
  MyVector<int> one_;
  MyVector<MyString> two_;

};

union UnionDoesNotWarn {
 UnionDoesNotWarn() = default;
 UnionDoesNotWarn(const UnionDoesNotWarn& other) = default;

 int a;
 int b;
 int c;
 int d;
 int e;
 int f;
 int g;
 int h;
 int i;
 int j;
 int k;
 int l;
 int m;
 int n;
 int o;
 int p;
 int q;
 int r;
 int s;
 int t;
 int u;
 int v;
 int w;
 int x;
 int y;
 int z;
};

class StringDoesNotWarn {
 public:
  StringDoesNotWarn() = default;
  ~StringDoesNotWarn() = default;

 private:
  std::string foo_;
};

class ThreeStringsDoesNotWarn {
 public:
  ThreeStringsDoesNotWarn() = default;
  ~ThreeStringsDoesNotWarn() = default;

 private:
  std::string one_;
  std::string two_;
  std::string three_;
};

class FourStringsWarns {
 public:
  FourStringsWarns() = default;
  ~FourStringsWarns() = default;

 private:
  std::string one_;
  std::string two_;
  std::string three_;
  std::string four_;
};

class CheckedPtrDoesNotWarn {
 public:
  CheckedPtrDoesNotWarn() = default;
  ~CheckedPtrDoesNotWarn() = default;

 private:
  CheckedPtr<CheckedPtrDoesNotWarn> foo_;
};

class NineCheckedPtrDoesNotWarn {
 public:
  NineCheckedPtrDoesNotWarn() = default;
  ~NineCheckedPtrDoesNotWarn() = default;

 private:
  CheckedPtr<NineCheckedPtrDoesNotWarn> one_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> two_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> three_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> four_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> five_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> six_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> seven_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> eight_;
  CheckedPtr<NineCheckedPtrDoesNotWarn> nine_;
};

class TenCheckedPtrWarns {
 public:
  TenCheckedPtrWarns() = default;
  ~TenCheckedPtrWarns() = default;

 private:
  CheckedPtr<TenCheckedPtrWarns> one_;
  CheckedPtr<TenCheckedPtrWarns> two_;
  CheckedPtr<TenCheckedPtrWarns> three_;
  CheckedPtr<TenCheckedPtrWarns> four_;
  CheckedPtr<TenCheckedPtrWarns> five_;
  CheckedPtr<TenCheckedPtrWarns> six_;
  CheckedPtr<TenCheckedPtrWarns> seven_;
  CheckedPtr<TenCheckedPtrWarns> eight_;
  CheckedPtr<TenCheckedPtrWarns> nine_;
  CheckedPtr<TenCheckedPtrWarns> ten_;
};

class RawPtrDoesNotWarn {
 public:
  RawPtrDoesNotWarn() = default;
  ~RawPtrDoesNotWarn() = default;

 private:
  raw_ptr<RawPtrDoesNotWarn> foo_;
};

class NineRawPtrDoesNotWarn {
 public:
  NineRawPtrDoesNotWarn() = default;
  ~NineRawPtrDoesNotWarn() = default;

 private:
  raw_ptr<NineRawPtrDoesNotWarn> one_;
  raw_ptr<NineRawPtrDoesNotWarn> two_;
  raw_ptr<NineRawPtrDoesNotWarn> three_;
  raw_ptr<NineRawPtrDoesNotWarn> four_;
  raw_ptr<NineRawPtrDoesNotWarn> five_;
  raw_ptr<NineRawPtrDoesNotWarn> six_;
  raw_ptr<NineRawPtrDoesNotWarn> seven_;
  raw_ptr<NineRawPtrDoesNotWarn> eight_;
  raw_ptr<NineRawPtrDoesNotWarn> nine_;
};

class TenRawPtrWarns {
 public:
  TenRawPtrWarns() = default;
  ~TenRawPtrWarns() = default;

 private:
  raw_ptr<TenRawPtrWarns> one_;
  raw_ptr<TenRawPtrWarns> two_;
  raw_ptr<TenRawPtrWarns> three_;
  raw_ptr<TenRawPtrWarns> four_;
  raw_ptr<TenRawPtrWarns> five_;
  raw_ptr<TenRawPtrWarns> six_;
  raw_ptr<TenRawPtrWarns> seven_;
  raw_ptr<TenRawPtrWarns> eight_;
  raw_ptr<TenRawPtrWarns> nine_;
  raw_ptr<TenRawPtrWarns> ten_;
};

#endif  // MISSING_CTOR_H_
