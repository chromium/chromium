// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/port_chromium.h"

#include <string>

#include "base/threading/platform_thread.h"
#include "third_party/crc32c/src/include/crc32c/crc32c.h"
#include "third_party/snappy/src/snappy.h"
#include "util/logging.h"

namespace leveldb {
namespace port {

Mutex::Mutex() = default;

Mutex::~Mutex() = default;

void Mutex::Lock() {
  mu_.Acquire();
}

void Mutex::Unlock() {
  mu_.Release();
}

void Mutex::AssertHeld() {
  mu_.AssertAcquired();
}

CondVar::CondVar(Mutex* mu)
    : cv_(&mu->mu_) {
}

CondVar::~CondVar() = default;

void CondVar::Wait() {
  cv_.Wait();
}

void CondVar::Signal(){
  cv_.Signal();
}

void CondVar::SignalAll() {
  cv_.Broadcast();
}

bool Snappy_Compress(const char* input,
                     size_t input_length,
                     std::string* output) {
  output->resize(snappy::MaxCompressedLength(input_length));
  size_t outlen;
  snappy::RawCompress(input, input_length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
}

bool Snappy_GetUncompressedLength(const char* input_data,
                                  size_t input_length,
                                  size_t* result) {
  return snappy::GetUncompressedLength(input_data, input_length, result);
}

bool Snappy_Uncompress(const char* input_data,
                       size_t input_length,
                       char* output) {
  return snappy::RawUncompress(input_data, input_length, output);
}

uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size) {
  return crc32c::Extend(crc, reinterpret_cast<const uint8_t*>(buf), size);
}

}  // namespace port
}  // namespace leveldb
