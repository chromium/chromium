//! Choose between std::sync::OnceLock and once_cell::race::OnceBox based on the
//! `std` feature.

#[cfg(feature = "std")]
pub(crate) type Once<T> = std::sync::OnceLock<T>;

#[cfg(not(feature = "std"))]
pub(crate) use once_impl::Once;

#[cfg(not(feature = "std"))]
mod once_impl {
    use alloc::boxed::Box;
    use once_cell::race::OnceBox;

    #[derive(Default)]
    pub struct Once<T>(OnceBox<T>);

    impl<T> Once<T> {
        pub const fn new() -> Self {
            Self(OnceBox::new())
        }

        pub fn get_or_init(&self, f: impl FnOnce() -> T) -> &T {
            self.0.get_or_init(|| Box::new(f()))
        }
    }
}
