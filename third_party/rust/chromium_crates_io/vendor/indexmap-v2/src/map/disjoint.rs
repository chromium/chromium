#![allow(unsafe_code)]

use crate::GetDisjointMutError;

/// Like `slice::get_disjoint_mut`, although we're not dealing with ranges (yet).
// TODO(MSRV 1.86): remove this in favor of the standard library's method.
pub(super) fn get_disjoint_mut<T, const N: usize>(
    entries: &mut [T],
    indices: [usize; N],
) -> Result<[&mut T; N], GetDisjointMutError> {
    // SAFETY: Can't allow duplicate indices as we would return mutable refs to the same data.
    let len = entries.len();
    for i in 0..N {
        let idx = indices[i];
        if idx >= len {
            return Err(GetDisjointMutError::IndexOutOfBounds);
        } else if indices[..i].contains(&idx) {
            return Err(GetDisjointMutError::OverlappingIndices);
        }
    }

    let entries_ptr = entries.as_mut_ptr();
    Ok(indices.map(move |idx| {
        // SAFETY: The base pointer is valid as it comes from a slice and the reference is always
        // in-bounds & unique as we've already checked the indices above.
        unsafe { &mut *(entries_ptr.add(idx)) }
    }))
}

/// Like `slice::get_disjoint_mut` but with optional indices,
/// allowing for absent keys from the user's original request.
#[track_caller]
pub(super) fn get_disjoint_opt_mut<T, const N: usize>(
    entries: &mut [T],
    indices: [Option<usize>; N],
) -> [Option<&mut T>; N] {
    // SAFETY: Can't allow duplicate indices as we would return mutable refs to the same data.
    let len = entries.len();
    for i in 0..N {
        if let Some(idx) = indices[i] {
            if idx >= len {
                unreachable!("`get_index_of` returned an out-of-bounds index");
            } else if indices[..i].contains(&Some(idx)) {
                panic!("duplicate keys found");
            }
        }
    }

    let entries_ptr = entries.as_mut_ptr();
    indices.map(move |idx_opt| {
        match idx_opt {
            Some(idx) => {
                // SAFETY: The base pointer is valid as it comes from a slice and the reference is always
                // in-bounds & unique as we've already checked the indices above.
                Some(unsafe { &mut *entries_ptr.add(idx) })
            }
            None => None,
        }
    })
}
