use better_any::Tid;
use better_any_derive::Tid;
struct S3<'a, T>(&'a T);
unsafe impl<'a, T> TidAble<'a> for S3<'a, T>
where
    T: TidAble<'a>,
{
    type Static = __S3aT_should_never_exist<T::Static>;
}
#[allow(warnings)]
#[doc(hidden)]
pub struct __S3aT_should_never_exist<T: ?Sized>(core::marker::PhantomData<T>);
struct S5<'a, T: Trait>(&'a T);
unsafe impl<'a, T: Trait> TidAble<'a> for S5<'a, T>
where
    T: TidAble<'a>,
{
    type Static = __S5aT_should_never_exist<T::Static>;
}
#[allow(warnings)]
#[doc(hidden)]
pub struct __S5aT_should_never_exist<T: ?Sized>(core::marker::PhantomData<T>);
struct S6<'a, T: TraitLT<'a>>(&'a T);
unsafe impl<'a, T: TraitLT<'a>> TidAble<'a> for S6<'a, T>
where
    T: TidAble<'a>,
{
    type Static = __S6aT_should_never_exist<T::Static>;
}
#[allow(warnings)]
#[doc(hidden)]
pub struct __S6aT_should_never_exist<T: ?Sized>(core::marker::PhantomData<T>);
struct S7<'a, T: 'static>(&'a T);
unsafe impl<'a, T: 'static> TidAble<'a> for S7<'a, T> {
    type Static = __S7aT_should_never_exist<T>;
}
#[allow(warnings)]
#[doc(hidden)]
pub struct __S7aT_should_never_exist<T: ?Sized>(core::marker::PhantomData<T>);
