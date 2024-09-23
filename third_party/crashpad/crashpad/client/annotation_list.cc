// Copyright 2017 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "client/annotation_list.h"

#include "base/check_op.h"
#include "client/crashpad_info.h"

namespace crashpad {

template <typename T>
T* AnnotationList::IteratorBase<T>::operator*() const {
  CHECK_NE(curr_, tail_);
  return curr_;
}

template <typename T>
T* AnnotationList::IteratorBase<T>::operator->() const {
  CHECK_NE(curr_, tail_);
  return curr_;
}

template <typename T>
AnnotationList::IteratorBase<T>& AnnotationList::IteratorBase<T>::operator++() {
  CHECK_NE(curr_, tail_);
  curr_ = curr_->GetLinkNode();
  return *this;
}

template <typename T>
AnnotationList::IteratorBase<T> AnnotationList::IteratorBase<T>::operator++(
    int) {
  T* const old_curr = curr_;
  ++(*this);
  return IteratorBase(old_curr, tail_);
}

template <typename T>
bool AnnotationList::IteratorBase<T>::operator!=(
    const IteratorBase& other) const {
  return !(*this == other);
}

template <typename T>
AnnotationList::IteratorBase<T>::IteratorBase(T* head, const Annotation* tail)
    : curr_(head), tail_(tail) {}

template class AnnotationList::IteratorBase<Annotation>;
template class AnnotationList::IteratorBase<const Annotation>;

AnnotationList::AnnotationList()
    : tail_pointer_(&tail_),
      head_(Annotation::Type::kInvalid, nullptr, nullptr),
      tail_(Annotation::Type::kInvalid, nullptr, nullptr) {
  head_.link_node().store(&tail_);
}

AnnotationList::~AnnotationList() {}

// static
AnnotationList* AnnotationList::Get() {
  return CrashpadInfo::GetCrashpadInfo()->annotations_list();
}

// static
AnnotationList* AnnotationList::Register() {
  AnnotationList* list = Get();
  if (!list) {
    list = new AnnotationList();
    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(list);
  }
  return list;
}

void AnnotationList::Add(Annotation* annotation) {
  Annotation* null = nullptr;
  Annotation* head_next = head_.link_node().load(std::memory_order_relaxed);
  if (!annotation->link_node().compare_exchange_strong(null, head_next)) {
    // If |annotation|'s link node is not null, then it has been added to the
    // list already and no work needs to be done.
    return;
  }

  // Check that the annotation's name is less than the maximum size. This is
  // done here, since the Annotation constructor must be constexpr and this
  // path is taken once per annotation.
  DCHECK_LT(strlen(annotation->name_), Annotation::kNameMaxLength);

  // Update the head link to point to the new |annotation|.
  while (!head_.link_node().compare_exchange_weak(head_next, annotation)) {
    // Another thread has updated the head-next pointer, so try again with the
    // re-loaded |head_next|.
    annotation->link_node().store(head_next, std::memory_order_relaxed);
  }
}

AnnotationList::Iterator AnnotationList::begin() {
  return Iterator(head_.GetLinkNode(), tail_pointer_);
}

AnnotationList::ConstIterator AnnotationList::cbegin() const {
  return ConstIterator(head_.GetLinkNode(), tail_pointer_);
}

AnnotationList::Iterator AnnotationList::end() {
  return Iterator(&tail_, tail_pointer_);
}

AnnotationList::ConstIterator AnnotationList::cend() const {
  return ConstIterator(&tail_, tail_pointer_);
}

}  // namespace crashpad
