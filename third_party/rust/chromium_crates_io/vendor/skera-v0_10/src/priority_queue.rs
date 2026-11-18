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
        let last = self.heap.pop()?;
        let Some(first) = self.heap.first_mut() else {
            return Some(last);
        };
        let ret = std::mem::replace(first, last);
        self.bubble_down(0);
        Some(ret)
    }

    #[inline]
    pub(crate) fn is_empty(&self) -> bool {
        self.heap.is_empty()
    }

    // Moves the item at `index` up to its place. This performs the same comparisons and
    // ends in the same heap layout as repeatedly swapping with the parent (as HarfBuzz
    // does), but moves the item through a "hole" instead of writing it at every level.
    #[inline]
    fn bubble_up(&mut self, index: usize) {
        let heap = self.heap.as_mut_slice();
        let item = heap[index];
        let mut cur_index = index;
        while cur_index != 0 {
            let parent_index = parent(cur_index);
            let parent_item = heap[parent_index];
            if parent_item.0 <= item.0 {
                break;
            }
            heap[cur_index] = parent_item;
            cur_index = parent_index;
        }
        heap[cur_index] = item;
    }

    // Moves the item at `index` down to its place, through a "hole" like bubble_up.
    //
    // Picks the same child as HarfBuzz (the left child unless the right one is smaller or
    // equal), but without a data-dependent branch: which child is smaller is close to random,
    // so a branch here is mispredicted about half of the time.
    #[inline]
    fn bubble_down(&mut self, index: usize) {
        let heap = self.heap.as_mut_slice();
        let length = heap.len();
        let item = heap[index];
        let mut cur_index = index;
        // Nodes with two children.
        loop {
            let left = left_child(cur_index);
            let right = left + 1;
            if right >= length {
                break;
            }
            let child = left + usize::from(heap[right].0 <= heap[left].0);
            let child_item = heap[child];
            if item.0 <= child_item.0 {
                heap[cur_index] = item;
                return;
            }
            heap[cur_index] = child_item;
            cur_index = child;
        }
        // A last node may have only a left child.
        let left = left_child(cur_index);
        if left < length && heap[left].0 < item.0 {
            heap[cur_index] = heap[left];
            cur_index = left;
        }
        heap[cur_index] = item;
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
