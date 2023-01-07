// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_SEQUENCED_QUEUE_H_
#define IPCZ_SRC_IPCZ_SEQUENCED_QUEUE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ipcz/sequence_number.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "util/safe_math.h"

namespace ipcz {

template <typename T>
struct DefaultSequencedQueueTraits {
  static size_t GetElementSize(const T& element) { return 0; }
};

// SequencedQueue retains a queue of objects strictly ordered by SequenceNumber.
// This class is not thread-safe.
//
// This is useful in situations where queued elements may accumulate slightly
// out-of-order and need to be reordered efficiently for consumption. The
// implementation relies on an assumption that sequence gaps are common but tend
// to be small and short-lived. As such, a SequencedQueue retains at least
// enough linear storage to hold every object between the last popped
// SequenceNumber (exclusive) and the highest queued (or anticipated)
// SequenceNumber so far (inclusive).
//
// Storage may be sparsely populated at times, but as elements are consumed from
// the queue, storage is compacted to reduce waste.
//
// ElementTraits may be overridden to attribute a measurable size to each stored
// element. SequencedQueue performs additional accounting to efficiently track
// the sum of this size across the set of all currently available elements in
// the queue.
template <typename T, typename ElementTraits = DefaultSequencedQueueTraits<T>>
class SequencedQueue {
 public:
  SequencedQueue() = default;

  // Constructs a new SequencedQueue starting at `initial_sequence_number`. The
  // queue will not accept elements with a SequenceNumber below this value, and
  // this will be the SequenceNumber of the first element to be popped.
  explicit SequencedQueue(SequenceNumber initial_sequence_number)
      : base_sequence_number_(initial_sequence_number) {}

  SequencedQueue(SequencedQueue&& other)
      : base_sequence_number_(other.base_sequence_number_),
        num_entries_(other.num_entries_),
        final_sequence_length_(other.final_sequence_length_) {
    if (!other.storage_.empty()) {
      size_t entries_offset = other.entries_.data() - storage_.data();
      storage_ = std::move(other.storage_);
      entries_ =
          EntryView(storage_.data() + entries_offset, other.entries_.size());
    }
  }

  SequencedQueue& operator=(SequencedQueue&& other) {
    base_sequence_number_ = other.base_sequence_number_;
    num_entries_ = other.num_entries_;
    final_sequence_length_ = other.final_sequence_length_;
    if (!other.storage_.empty()) {
      size_t entries_offset = other.entries_.data() - storage_.data();
      storage_ = std::move(other.storage_);
      entries_ =
          EntryView(storage_.data() + entries_offset, other.entries_.size());
    } else {
      storage_.clear();
      entries_ = EntryView(storage_.data(), 0);
    }
    return *this;
  }

  ~SequencedQueue() = default;

  // As a basic practical constraint, SequencedQueue won't tolerate sequence
  // gaps larger than this value.
  static constexpr uint64_t GetMaxSequenceGap() { return 1000000; }

  // The SequenceNumber of the next element that is or will be available from
  // the queue. This starts at the constructor's `initial_sequence_number` and
  // increments any time an element is successfully popped from the queue or a
  // a SequenceNumber is explicitly skipped via SkipNextSequenceNumber().
  SequenceNumber current_sequence_number() const {
    return base_sequence_number_;
  }

  // The total size of all element from this queue so far.
  uint64_t total_consumed_element_size() const {
    return total_consumed_element_size_;
  }

  // The final length of the sequence that can be popped from this queue. Null
  // if a final length has not yet been set. If the final length is N, then the
  // last ordered element that can be pushed to or popped from the queue has a
  // SequenceNumber of N-1.
  const absl::optional<SequenceNumber>& final_sequence_length() const {
    return final_sequence_length_;
  }

  // Returns the number of elements currently ready for popping at the front of
  // the queue. This is the number of contiguously sequenced elements
  // available starting from `current_sequence_number()`. If the current
  // SequenceNumber is 5 and this queue holds elements 5, 6, and 8, then this
  // method returns 2: only elements 5 and 6 are available, because element 8
  // cannot be made available until element 7 is also available.
  size_t GetNumAvailableElements() const {
    if (entries_.empty() || !entries_[0].has_value()) {
      return 0;
    }

    return entries_[0]->num_entries_in_span;
  }

  // Returns the total size of elements currently ready for popping at the
  // front of the queue. This is the sum of ElementTraits::GetElementSize() for
  // each element counted by `GetNumAvailableElements()`, and it is always
  // returned in constant time.
  size_t GetTotalAvailableElementSize() const {
    if (entries_.empty() || !entries_[0].has_value()) {
      return 0;
    }

    return entries_[0]->total_span_size;
  }

  // Returns the total size of all elements previously popped from this queue,
  // plus the total size of all elemenets currently ready for popping.
  uint64_t GetTotalElementSizeQueuedSoFar() const {
    return CheckAdd(total_consumed_element_size_,
                    static_cast<uint64_t>(GetTotalAvailableElementSize()));
  }

  // Returns the total length of the contiguous sequence already pushed and/or
  // popped from this queue so far. This is essentially
  // `current_sequence_number()` plus `GetNumAvailableElements()`. If
  // `current_sequence_number()` is 5 and `GetNumAvailableElements()` is 3, then
  // elements 5, 6, and 7 are available for retrieval and the current sequence
  // length is 8; so this method would return 8.
  SequenceNumber GetCurrentSequenceLength() const {
    return SequenceNumber{current_sequence_number().value() +
                          GetNumAvailableElements()};
  }

  // Sets the final length of this queue's sequence. This is the SequenceNumber
  // of the last element that can be pushed, plus 1. If this is set to zero, no
  // elements can ever be pushed onto this queue.
  //
  // May fail and return false if the queue already has pushed and/or popped
  // elements with a SequenceNumber greater than or equal to `length`, or if a
  // the final sequence length had already been set prior to this call.
  bool SetFinalSequenceLength(SequenceNumber length) {
    if (final_sequence_length_) {
      return false;
    }

    const SequenceNumber lower_bound(base_sequence_number_.value() +
                                     entries_.size());
    if (length < lower_bound) {
      return false;
    }

    if (length.value() - base_sequence_number_.value() > GetMaxSequenceGap()) {
      return false;
    }

    final_sequence_length_ = length;
    return Reallocate(length);
  }

  // Forcibly sets the final length of this queue's sequence to its currently
  // available length. This means that if there is a gap in the available
  // elements, the queue is cut off just before the gap and all elements beyond
  // the gap are destroyed. If the final sequence length had already been set on
  // this queue, this overrides that.
  void ForceTerminateSequence() {
    final_sequence_length_ = GetCurrentSequenceLength();
    num_entries_ = GetNumAvailableElements();
    if (num_entries_ == 0) {
      storage_.clear();
      entries_ = {};
      return;
    }

    const size_t entries_offset = entries_.data() - storage_.data();
    storage_.resize(entries_offset + num_entries_);
    entries_ = EntryView(storage_.data() + entries_offset, num_entries_);
  }

  // Indicates whether this queue is still expecting to have more elements
  // pushed. This is always true if the final sequence length has not been set
  // yet.
  //
  // Once the final sequence length is set, this remains true only until all
  // elements between the initial sequence number (inclusive) and the final
  // sequence length (exclusive) have been pushed into the queue.
  bool ExpectsMoreElements() const {
    if (!final_sequence_length_) {
      return true;
    }

    if (base_sequence_number_ >= *final_sequence_length_) {
      return false;
    }

    const size_t num_entries_remaining =
        final_sequence_length_->value() - base_sequence_number_.value();
    return num_entries_ < num_entries_remaining;
  }

  // Indicates whether the next element (in sequence order) is available to pop.
  bool HasNextElement() const {
    return !entries_.empty() && entries_[0].has_value();
  }

  // Indicates whether this queue's sequence has been fully consumed. This means
  // the final sequence length has been set AND all elements up to that length
  // have been pushed into and popped from the queue.
  bool IsSequenceFullyConsumed() const {
    return !HasNextElement() && !ExpectsMoreElements();
  }

  // Resets this queue to a state which behaves as if a sequence of parcels of
  // length `n` has already been pushed and popped from the queue, with a total
  // cumulative element size of `total_consumed_element_size`. Must be called
  // only on an empty queue and only when the caller can be sure they won't want
  // to push any elements with a SequenceNumber below `n`.
  void ResetSequence(SequenceNumber n, uint64_t total_consumed_element_size) {
    ABSL_ASSERT(num_entries_ == 0);
    base_sequence_number_ = n;
    total_consumed_element_size_ = total_consumed_element_size;
  }

  // Attempts to skip SequenceNumber `n` in the sequence by advancing the
  // current SequenceNumber by one. Returns true on success and false on
  // failure. `element_size` is the size of the skipped element as it would have
  // been reported by ElementTraits::GetElementSize() if the element in question
  // were actually pushed into the queue.
  //
  // This can only succeed when `current_sequence_number()` is equal to `n`, no
  // entry for SequenceNumber `n` is already in the queue, and n` is less than
  // the final sequence length if applicable. Success is equivalent to pushing
  // and immediately popping element `n` except that it does not grow, shrink,
  // or otherwise modify the queue's underlying storage.
  bool SkipElement(SequenceNumber n, size_t element_size) {
    if (base_sequence_number_ != n || HasNextElement() ||
        (final_sequence_length_ && *final_sequence_length_ <= n)) {
      return false;
    }

    base_sequence_number_ = SequenceNumber{n.value() + 1};
    if (num_entries_ != 0) {
      entries_.remove_prefix(1);
    }
    total_consumed_element_size_ = CheckAdd(
        total_consumed_element_size_, static_cast<uint64_t>(element_size));
    return true;
  }

  // Pushes an element into the queue with the given SequenceNumber. This may
  // fail if `n` falls below the minimum or maximum (when applicable) expected
  // sequence number for elements in this queue.
  bool Push(SequenceNumber n, T element) {
    if (n < base_sequence_number_ ||
        (n.value() - base_sequence_number_.value() > GetMaxSequenceGap())) {
      return false;
    }

    // Compute the appropriate index at which to store this new entry, given its
    // SequenceNumber and the base SequenceNumber of element 0 in `entries_`.
    size_t index = n.value() - base_sequence_number_.value();
    if (final_sequence_length_) {
      // If `final_sequence_length_` is set, `entries_` must already be sized
      // large enough to hold any valid Push().
      if (index >= entries_.size() || entries_[index].has_value()) {
        // Out of bounds or duplicate entry. Fail.
        return false;
      }
      PlaceNewEntry(index, n, element);
      return true;
    }

    if (index < entries_.size()) {
      // `entries_` is already large enough to place this element without
      // resizing.
      if (entries_[index].has_value()) {
        // Duplicate entry. Fail.
        return false;
      }
      PlaceNewEntry(index, n, element);
      return true;
    }

    SequenceNumber new_limit(n.value() + 1);
    if (new_limit == SequenceNumber(0)) {
      return false;
    }

    if (!Reallocate(new_limit)) {
      return false;
    }

    PlaceNewEntry(index, n, element);
    return true;
  }

  // Pops the next (in sequence order) element off the queue if available,
  // populating `element` with its contents and returning true on success. On
  // failure `element` is untouched and this returns false.
  bool Pop(T& element) {
    if (entries_.empty() || !entries_[0].has_value()) {
      return false;
    }

    Entry& head = *entries_[0];
    element = std::move(head.element);

    ABSL_ASSERT(num_entries_ > 0);
    --num_entries_;
    const SequenceNumber sequence_number = base_sequence_number_;
    base_sequence_number_ = SequenceNumber{base_sequence_number_.value() + 1};

    // Make sure the next queued entry has up-to-date accounting, if present.
    const size_t element_size = ElementTraits::GetElementSize(element);
    if (entries_.size() > 1 && entries_[1]) {
      Entry& next = *entries_[1];
      next.span_start = head.span_start;
      next.span_end = head.span_end;
      next.num_entries_in_span = head.num_entries_in_span - 1;
      next.total_span_size = head.total_span_size - element_size;

      size_t tail_index = next.span_end.value() - sequence_number.value();
      if (tail_index > 1) {
        Entry& tail = *entries_[tail_index];
        tail.num_entries_in_span = next.num_entries_in_span;
        tail.total_span_size = next.total_span_size;
      }
    }

    entries_[0].reset();
    entries_ = entries_.subspan(1);

    // If there's definitely no more populated element data, take this
    // opportunity to realign `entries_` to the front of `storage_` to reduce
    // future allocations.
    if (num_entries_ == 0) {
      entries_ = EntryView(storage_.data(), entries_.size());
    }

    total_consumed_element_size_ = CheckAdd(
        total_consumed_element_size_, static_cast<uint64_t>(element_size));
    return true;
  }

  // Gets a reference to the next element. This reference is NOT stable across
  // any non-const methods here.
  T& NextElement() {
    ABSL_ASSERT(HasNextElement());
    return entries_[0]->element;
  }

 protected:
  // Adjusts the recorded size of the element at the head of this queue, as if
  // the element were partially consumed. After this call, the value returned by
  // GetTotalAvailableElementSize() will be decreased by `amount`, and the value
  // returned by total_consumed_element_size() will increase by the same.
  void PartiallyConsumeNextElement(size_t amount) {
    ABSL_ASSERT(HasNextElement());
    ABSL_ASSERT(entries_[0]->total_span_size >= amount);
    entries_[0]->total_span_size -= amount;
    total_consumed_element_size_ =
        CheckAdd(total_consumed_element_size_, static_cast<uint64_t>(amount));
  }

 private:
  bool Reallocate(SequenceNumber sequence_length) {
    if (sequence_length < base_sequence_number_) {
      return false;
    }

    uint64_t new_entries_size =
        sequence_length.value() - base_sequence_number_.value();
    if (new_entries_size > GetMaxSequenceGap()) {
      return false;
    }

    size_t entries_offset = entries_.data() - storage_.data();
    if (storage_.size() - entries_offset > new_entries_size) {
      // Fast path: just extend the view into storage.
      entries_ = EntryView(storage_.data() + entries_offset, new_entries_size);
      return true;
    }

    // We need to reallocate storage. Re-align `entries_` with the front of the
    // buffer, and leave some extra room when allocating.
    if (entries_offset > 0) {
      for (size_t i = 0; i < entries_.size(); ++i) {
        storage_[i] = std::move(entries_[i]);
        entries_[i].reset();
      }
    }

    storage_.resize(new_entries_size * 2);
    entries_ = EntryView(storage_.data(), new_entries_size);
    return true;
  }

  // See detailed comments on Entry below for an explanation of this logic.
  void PlaceNewEntry(size_t index, SequenceNumber n, T& element) {
    ABSL_ASSERT(index < entries_.size());
    ABSL_ASSERT(!entries_[index].has_value());

    entries_[index].emplace();
    Entry& entry = *entries_[index];
    entry.num_entries_in_span = 1;
    entry.total_span_size = ElementTraits::GetElementSize(element);

    entry.element = std::move(element);

    if (index == 0 || !entries_[index - 1]) {
      entry.span_start = n;
    } else {
      Entry& left = *entries_[index - 1];
      entry.span_start = left.span_start;
      entry.num_entries_in_span += left.num_entries_in_span;
      entry.total_span_size += left.total_span_size;
    }

    if (index == entries_.size() - 1 || !entries_[index + 1]) {
      entry.span_end = n;
    } else {
      Entry& right = *entries_[index + 1];
      entry.span_end = right.span_end;
      entry.num_entries_in_span += right.num_entries_in_span;
      entry.total_span_size += right.total_span_size;
    }

    Entry* start;
    if (entry.span_start <= base_sequence_number_) {
      start = &entries_[0].value();
    } else {
      const size_t start_index =
          entry.span_start.value() - base_sequence_number_.value();
      start = &entries_[start_index].value();
    }

    ABSL_ASSERT(entry.span_end >= base_sequence_number_);
    const size_t end_index =
        entry.span_end.value() - base_sequence_number_.value();
    ABSL_ASSERT(end_index < entries_.size());
    Entry* end = &entries_[end_index].value();

    start->span_end = entry.span_end;
    start->num_entries_in_span = entry.num_entries_in_span;
    start->total_span_size = entry.total_span_size;

    end->span_start = entry.span_start;
    end->num_entries_in_span = entry.num_entries_in_span;
    end->total_span_size = entry.total_span_size;

    ++num_entries_;
  }

  struct Entry {
    Entry() = default;
    Entry(Entry&& other) = default;
    Entry& operator=(Entry&&) = default;
    ~Entry() = default;

    T element;

    // NOTE: The fields below are maintained during Push and Pop operations and
    // are used to support efficient implementation of GetNumAvailableElements()
    // and GetTotalAvailableElementSize(). This warrants some clarification.
    //
    // Conceptually we treat the active range of entries as a series of
    // contiguous spans:
    //
    //     `entries_`: [2][ ][4][5][6][ ][8][9]
    //
    // For example, above we can designate three contiguous spans: element 2
    // stands alone at the front of the queue, elements 4-6 form a second span,
    // and then elements 8-9 form the third. Elements 3 and 7 are absent.
    //
    // We're interested in knowing how many elements (and their total size, as
    // designated by ElementTraits) are available right now, which means we want
    // to answer the question: how long is the span starting at element 0? In
    // this case since element 2 stands alone at the front of the queue, the
    // answer is 1. There's 1 element available right now.
    //
    // If we pop element 2 off the queue, it then becomes:
    //
    //     `entries_`: [ ][4][5][6][ ][8][9]
    //
    // The head of the queue is pointing at the empty slot for element 3, and
    // because no span starts in element 0 there are now 0 elements available to
    // pop.
    //
    // Finally if we then push element 3, the queue looks like this:
    //
    //     `entries_`: [3][4][5][6][ ][8][9]
    //
    // and now there are 4 elements available to pop. Element 0 begins the span
    // of elements 3, 4, 5, and 6.
    //
    // To answer the question efficiently though, each entry records some
    // metadata about the span in which it resides. This information is not kept
    // up-to-date for all entries, but we maintain the invariant that the first
    // and last element of each distinct span has accurate metadata; and as a
    // consequence if any span starts at element 0, then we know element 0's
    // metadata accurately answers our general questions about the head of the
    // queue.
    //
    // When an element with sequence number N is inserted into the queue, it can
    // be classified in one of four ways:
    //
    //    (1) it stands alone with no element present at N-1 or N+1
    //    (2) it follows an element at N-1, but N+1 is empty
    //    (3) it precedes an element at N+1, but N-1 is empty
    //    (4) it falls between present elements at both N-1 and N+1.
    //
    // In case (1) we record in the entry that its span starts and ends at
    // element N; we also record the length of the span (1) and a traits-defined
    // accounting of the element's "size". This entry now has trivially correct
    // metadata about its containing span, which consists only of the entry
    // itself.
    //
    // In case (2), element N is now the tail of a pre-existing span. Because
    // tail elements are always up-to-date, we simply copy and augment the data
    // from the old tail (element N-1) into the new tail (element N). From this
    // data we also know where the head of the span is, and the head entry is
    // also updated to reflect the same new metadata.
    //
    // Case (3) is similar to case (2). Element N is now the head of a
    // pre-existing span, so we copy and augment the already up-to-date N+1
    // entry's metadata (the old head) into our new entry as well as the span's
    // tail entry.
    //
    // Case (4) is joining two pre-existing spans. In this case element N
    // fetches the span's start from element N-1 (the tail of the span to the
    // left), and the span's end from element N+1 (the head of the span to the
    // right); and it sums their element and byte counts with its own. This new
    // combined metadata is copied into both the head of the left span and the
    // tail of the right span, and with element N populated this now constitutes
    // a single combined span with accurate metadata in its head and tail
    // entries.
    //
    // Finally, the only other operation that matters for this accounting is
    // Pop(). All Pop() needs to do is derive new metadata for the new
    // head-of-queue's span (if present) after popping. This metadata is updated
    // in the new head entry as well as its span's tail.
    size_t num_entries_in_span = 0;
    size_t total_span_size = 0;
    SequenceNumber span_start{0};
    SequenceNumber span_end{0};
  };

  using EntryStorage = absl::InlinedVector<absl::optional<Entry>, 4>;
  using EntryView = absl::Span<absl::optional<Entry>>;

  // This is a sparse vector of queued elements indexed by a relative sequence
  // number.
  //
  // It's sparse because the queue may push elements out of sequence order (e.g.
  // elements 42 and 47 may be pushed before elements 43-46.)
  EntryStorage storage_;

  // A view into `storage_` whose first element corresponds to the entry with
  // sequence number `base_sequence_number_`. As elements are popped, the view
  // moves forward in `storage_`. When convenient, we may reallocate `storage_`
  // and realign this view.
  EntryView entries_{storage_.data(), 0};

  // The sequence number which corresponds to `entries_` index 0 when `entries_`
  // is non-empty.
  SequenceNumber base_sequence_number_{0};

  // The number of slots in `entries_` which are actually occupied.
  size_t num_entries_ = 0;

  // Tracks the sum of the element sizes of every element fully or partially
  // consumed from the queue so far.
  uint64_t total_consumed_element_size_ = 0;

  // The final length of the sequence to be enqueued, if known.
  absl::optional<SequenceNumber> final_sequence_length_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_SEQUENCED_QUEUE_H_
