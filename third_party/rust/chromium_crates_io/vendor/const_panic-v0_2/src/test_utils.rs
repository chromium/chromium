use core::{
    cmp::PartialEq,
    fmt::{self, Debug},
};

pub struct TestString<const LEN: usize> {
    pub(crate) len: usize,
    pub(crate) buffer: [u8; LEN],
}

impl<const LEN: usize> TestString<LEN> {
    pub fn get(&self) -> &[u8] {
        &self.buffer[..self.len]
    }
    pub fn as_str(&self) -> &str {
        core::str::from_utf8(self.get()).unwrap()
    }
}

impl<const LEN: usize> Debug for TestString<LEN> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match core::str::from_utf8(self.get()) {
            Ok(x) => Debug::fmt(x, f),
            Err(_) => f
                .debug_struct("TestString")
                .field("len", &self.len)
                .field("buffer", &self.buffer)
                .finish(),
        }
    }
}

impl<const LEN: usize> PartialEq<str> for TestString<LEN> {
    fn eq(&self, str: &str) -> bool {
        core::str::from_utf8(self.get()).map_or(false, |this| this == str)
    }
}
impl<const LEN: usize> PartialEq<&str> for TestString<LEN> {
    fn eq(&self, str: &&str) -> bool {
        self == *str
    }
}
#[doc(hidden)]
#[macro_export]
macro_rules! concat_fmt {
    ($length:expr, $max_len:expr; $($args:tt)*) => (
        $crate::__concat_func_setup!{
            (|args| $crate::for_tests::format_panic_message::<1024>(args, $length, $max_len))
            []
            [$($args)*,]
        }
    )
}

// workaround for PhantomData changing to printing type argument
pub struct MyPhantomData<T: ?Sized>(core::marker::PhantomData<T>);

impl<T: ?Sized> Copy for MyPhantomData<T> {}

impl<T: ?Sized> Clone for MyPhantomData<T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T: ?Sized> core::fmt::Debug for MyPhantomData<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        f.write_str("PhantomData")
    }
}

impl<T: ?Sized> crate::PanicFmt for MyPhantomData<T> {
    type This = Self;
    type Kind = crate::IsCustomType;
    const PV_COUNT: usize = 1;
}

impl<T: ?Sized> MyPhantomData<T> {
    pub const NEW: Self = Self(core::marker::PhantomData);

    pub const fn to_panicvals(self, _: crate::FmtArg) -> [crate::PanicVal<'static>; 1] {
        [crate::PanicVal::write_str("PhantomData")]
    }
}
