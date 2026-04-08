/// ###################################################
///
/// Ensure that phantom type parameters must be ignored
///
/// ```compile_fail
/// struct Foo<T>(std::marker::PhantomData<T>);
///
/// const_panic::impl_panicfmt!{
///     struct Foo<T>(std::marker::PhantomData<T>);
/// }
/// ```
///
/// ```rust
/// struct Foo<T>(std::marker::PhantomData<T>);
///
/// const_panic::impl_panicfmt!{
///     struct Foo<ignore T>(std::marker::PhantomData<T>);
/// }
/// ```
///
///
pub struct ImplPanicFmt;
