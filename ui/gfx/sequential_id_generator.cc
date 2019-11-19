// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/sequential_id_generator.h"

#include "base/logging.h"

namespace {

// Removes |key| from |first|, and |first[key]| from |second|.
template <typename T>
void Remove(uint32_t key, T* first, T* second) {
  auto iter = first->find(key);
  if (iter == first->end())
    return;

  uint32_t second_key = iter->second;
  first->erase(iter);

  iter = second->find(second_key);
  DCHECK(iter != second->end());
  second->erase(iter);
}

}  // namespace

namespace ui {

SequentialIDGenerator::SequentialIDGenerator(uint32_t min_id)
    : min_id_(min_id), min_available_id_(min_id) {}

SequentialIDGenerator::~SequentialIDGenerator() {
}

uint32_t SequentialIDGenerator::GetGeneratedID(uint32_t number) {
  auto find = number_to_id_.find(number);
  if (find != number_to_id_.end())
    return find->second;

  int id = GetNextAvailableID();
  number_to_id_.emplace(number, id);
  id_to_number_.emplace(id, number);
  return id;
}

bool SequentialIDGenerator::HasGeneratedIDFor(uint32_t number) const {
  return number_to_id_.find(number) != number_to_id_.end();
}

void SequentialIDGenerator::ReleaseNumber(uint32_t number) {
  if (number_to_id_.count(number) > 0U) {
    UpdateNextAvailableIDAfterRelease(number_to_id_[number]);
    Remove(number, &number_to_id_, &id_to_number_);
  }
}

void SequentialIDGenerator::ReleaseID(uint32_t id) {
  if (id_to_number_.count(id) > 0U) {
    UpdateNextAvailableIDAfterRelease(id);
    Remove(id_to_number_[id], &number_to_id_, &id_to_number_);
  }
}

void SequentialIDGenerator::ResetForTest() {
  number_to_id_.clear();
  id_to_number_.clear();
  min_available_id_ = min_id_;
}

uint32_t SequentialIDGenerator::GetNextAvailableID() {
  const uint32_t kMaxID = 128;
  while (id_to_number_.count(min_available_id_) > 0 &&
         min_available_id_ < kMaxID) {
    ++min_available_id_;
  }
  if (min_available_id_ >= kMaxID)
    min_available_id_ = min_id_;
  return min_available_id_;
}

void SequentialIDGenerator::UpdateNextAvailableIDAfterRelease(uint32_t id) {
  if (id < min_available_id_) {
    min_available_id_ = id;
    DCHECK_GE(min_available_id_, min_id_);
  }
}

}  // namespace ui
