use core::cmp::Ordering;

/// Sorts `items` by `compare`, keeping equal items in their original order.
///
/// An insertion sort, so quadratic: for sequences known to be short, as
/// HarfBuzz uses `hb_stable_sort`.
pub(crate) fn stable_sort_by<T: Copy>(
    items: &mut [T],
    mut compare: impl FnMut(&T, &T) -> Ordering,
) {
    for i in 1..items.len() {
        let item = items[i];
        let mut j = i;
        while j > 0 && compare(&items[j - 1], &item).is_gt() {
            items[j] = items[j - 1];
            j -= 1;
        }
        items[j] = item;
    }
}

/// Sorts `items` by `compare`, without allocating and without keeping equal
/// items in their original order.
///
/// A heap sort compiles to far less code per item type than the standard
/// library's sorts, and the sorts done while building a plan are small.
pub(crate) fn sort_unstable_by<T>(items: &mut [T], mut compare: impl FnMut(&T, &T) -> Ordering) {
    let len = items.len();
    for root in (0..len / 2).rev() {
        sift_down(items, root, len, &mut compare);
    }
    for end in (1..len).rev() {
        items.swap(0, end);
        sift_down(items, 0, end, &mut compare);
    }
}

fn sift_down<T>(
    items: &mut [T],
    mut root: usize,
    end: usize,
    compare: &mut impl FnMut(&T, &T) -> Ordering,
) {
    loop {
        let mut child = 2 * root + 1;
        if child >= end {
            break;
        }
        if child + 1 < end && compare(&items[child], &items[child + 1]).is_lt() {
            child += 1;
        }
        if !compare(&items[root], &items[child]).is_lt() {
            break;
        }
        items.swap(root, child);
        root = child;
    }
}

// FLAG macro in harfbuzz.
#[inline]
pub const fn rb_flag(x: u32) -> u32 {
    1 << x
}

// FLAG_UNSAFE macro in harfbuzz.
#[inline]
pub fn rb_flag_unsafe(x: u32) -> u32 {
    if x < 32 {
        1 << x
    } else {
        0
    }
}

// FLAG64 macro in harfbuzz.
#[inline]
pub const fn rb_flag64(x: u32) -> u64 {
    1 << x as u64
}

// FLAG64_UNSAFE macro in harfbuzz.
#[inline]
pub fn rb_flag64_unsafe(x: u32) -> u64 {
    if x < 64 {
        1 << (x as u64)
    } else {
        0
    }
}

/* Encodes three unsigned integers in one 64-bit number.  If the inputs have more than 21 bits,
 * values will be truncated / overlap, and might not decode exactly. */
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_ENCODE3(x: u32, y: u32, z: u32) -> u64 {
    ((x as u64) << 42) | ((y as u64) << 21) | (z as u64)
}
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_DECODE3_1(v: u64) -> u32 {
    (v >> 42) as u32
}
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_DECODE3_2(v: u64) -> u32 {
    ((v >> 21) & 0x001F_FFFF) as u32
}
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_DECODE3_3(v: u64) -> u32 {
    (v & 0x001F_FFFF) as u32
}

/* Custom encoding used by hb-ucd. */
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_ENCODE3_11_7_14(x: u32, y: u32, z: u32) -> u32 {
    ((x & 0x07FF) << 21) | ((y & 0x007F) << 14) | (z & 0x3FFF)
}
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_DECODE3_11_7_14_1(v: u32) -> u32 {
    v >> 21
}
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_DECODE3_11_7_14_2(v: u32) -> u32 {
    ((v >> 14) & 0x007F) | 0x0300
}
#[allow(non_snake_case, dead_code)]
pub const fn HB_CODEPOINT_DECODE3_11_7_14_3(v: u32) -> u32 {
    v & 0x3FFF
}

#[cfg(test)]
mod tests {
    use super::{sort_unstable_by, stable_sort_by};
    use alloc::vec::Vec;

    #[test]
    fn heap_sort_matches_std_sort() {
        for len in 0..200 {
            let mut items: Vec<u32> = (0..len).map(|i| (i * 173 + 31) % 37).collect();
            let mut expected = items.clone();
            expected.sort_unstable();
            sort_unstable_by(&mut items, Ord::cmp);
            assert_eq!(items, expected);
        }
    }

    #[test]
    fn insertion_sort_is_stable() {
        for len in 0..64 {
            let mut items: Vec<(u32, u32)> = (0..len).map(|i| ((i * 7 + 3) % 5, i)).collect();
            let mut expected = items.clone();
            expected.sort_by_key(|item| item.0);
            stable_sort_by(&mut items, |a, b| a.0.cmp(&b.0));
            assert_eq!(items, expected);
        }
    }
}
