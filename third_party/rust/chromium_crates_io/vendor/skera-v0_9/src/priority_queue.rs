//! Implement a priority queue which supports extract minimum and insert operations
//! ref: <https://github.com/harfbuzz/harfbuzz/blob/9cfb0e6786ecceabaec7a26fd74b1ddb1209f74d/src/hb-priority-queue.hh#L46>
//!
//! The priority queue is implemented as a binary heap, which is a complete
//! The heap property is that the priority of a node is less than or equal to the
//! priority of its children. The heap is stored in an array, with the
//! children of node i stored at indices 2i + 1 and 2i + 2.

pub(crate) struct PriorityQueue<T: Copy + PartialOrd> {
    heap: Vec<(T, usize)>,
}

impl<T: Copy + PartialOrd> PriorityQueue<T> {
    #[inline]
    pub(crate) fn with_capacity(capacity: usize) -> Self {
        Self {
            heap: Vec::with_capacity(capacity),
        }
    }

    #[inline]
    pub(crate) fn push(&mut self, item: (T, usize)) {
        self.heap.push(item);
        self.bubble_up(self.heap.len() - 1);
    }

    #[inline]
    pub(crate) fn pop(&mut self) -> Option<(T, usize)> {
        if self.heap.is_empty() {
            return None;
        }

        let ret = self.heap[0];
        let new_len = self.heap.len() - 1;
        self.heap.swap(0, new_len);
        self.heap.truncate(new_len);

        if new_len != 0 {
            self.bubble_down(0);
        }
        Some(ret)
    }

    #[inline]
    pub(crate) fn is_empty(&self) -> bool {
        self.heap.is_empty()
    }

    #[inline]
    fn bubble_up(&mut self, index: usize) {
        let length = self.heap.len();
        let mut cur_index = index;
        while cur_index != 0 {
            assert!(cur_index < length);

            let parent_index = parent(cur_index);
            assert!(parent_index < length);
            if self.heap[parent_index].0 <= self.heap[cur_index].0 {
                return;
            }

            self.heap.swap(cur_index, parent_index);
            cur_index = parent_index;
        }
    }

    #[inline]
    fn bubble_down(&mut self, index: usize) {
        let length = self.heap.len();
        let mut cur_index = index;
        loop {
            assert!(cur_index < length);

            let left = left_child(cur_index);
            if left >= length {
                return;
            }

            let right = right_child(cur_index);
            let has_right_child = right < length;

            let val = self.heap[cur_index].0;
            let left_val = self.heap[left].0;
            if val <= left_val && (!has_right_child || val <= self.heap[right].0) {
                return;
            }

            let child = if !has_right_child || left_val < self.heap[right].0 {
                left
            } else {
                right
            };

            self.heap.swap(cur_index, child);
            cur_index = child;
        }
    }
}

#[inline]
fn parent(child_index: usize) -> usize {
    (child_index - 1) / 2
}

#[inline]
fn left_child(index: usize) -> usize {
    2 * index + 1
}

#[inline]
fn right_child(index: usize) -> usize {
    2 * index + 2
}

#[cfg(test)]
mod test {
    use super::*;

    impl<T: Copy + PartialOrd> PriorityQueue<T> {
        fn minimum(&self) -> Option<&(T, usize)> {
            self.heap.first()
        }
    }
    #[test]
    fn test_push() {
        let mut queue = PriorityQueue::with_capacity(10);
        assert!(queue.is_empty());
        assert_eq!(queue.minimum(), None);

        queue.push((10, 0));
        assert!(!queue.is_empty());
        assert_eq!(queue.minimum(), Some(&(10, 0)));

        queue.push((20, 1));
        assert_eq!(queue.minimum(), Some(&(10, 0)));

        queue.push((5, 2));
        assert_eq!(queue.minimum(), Some(&(5, 2)));

        queue.push((15, 3));
        assert_eq!(queue.minimum(), Some(&(5, 2)));

        queue.push((1, 4));
        assert_eq!(queue.minimum(), Some(&(1, 4)));
    }

    #[test]
    fn test_pop() {
        let mut queue = PriorityQueue::with_capacity(8);
        queue.push((0, 0));
        queue.push((60, 6));
        queue.push((30, 3));
        queue.push((40, 4));
        queue.push((20, 2));
        queue.push((50, 5));
        queue.push((70, 7));
        queue.push((10, 1));

        for i in 0..8 {
            assert!(!queue.is_empty());
            assert_eq!(queue.minimum(), Some(&(i * 10, i)));
            assert_eq!(queue.pop(), Some((i * 10, i)));
        }
    }
}
