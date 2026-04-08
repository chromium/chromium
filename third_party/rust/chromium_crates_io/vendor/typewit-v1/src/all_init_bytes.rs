#[cfg(test)]
mod tests;

/// Marker trait for type for which all bytes are initialized
/// 
/// # Safety
/// 
/// All of the bytes in this type must be initialized.
/// Uninitialized bytes includes padding and `MaybeUninit` fields.
pub(crate) unsafe trait AllInitBytes {}

#[allow(dead_code)]
pub(crate) const fn as_bytes<T: AllInitBytes>(reff: &T) -> &[u8] {
    unsafe {
        core::slice::from_raw_parts (
            reff as *const T as *const u8,
            core::mem::size_of::<T>(),
        )
    }
}
pub(crate) const fn slice_as_bytes<T: AllInitBytes>(reff: &[T]) -> &[u8] {
    unsafe {
        core::slice::from_raw_parts (
            reff.as_ptr() as *const u8,
            reff.len() * core::mem::size_of::<T>(),
        )
    }
}

unsafe impl AllInitBytes for bool {}
unsafe impl AllInitBytes for char {}
unsafe impl AllInitBytes for u8 {}
unsafe impl AllInitBytes for u16 {}
unsafe impl AllInitBytes for u32 {}
unsafe impl AllInitBytes for u64 {}
unsafe impl AllInitBytes for u128 {}
unsafe impl AllInitBytes for usize {}
unsafe impl AllInitBytes for i8 {}
unsafe impl AllInitBytes for i16 {}
unsafe impl AllInitBytes for i32 {}
unsafe impl AllInitBytes for i64 {}
unsafe impl AllInitBytes for i128 {}
unsafe impl AllInitBytes for isize {}

