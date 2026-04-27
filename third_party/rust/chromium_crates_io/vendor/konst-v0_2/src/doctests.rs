///
/// ```
/// use konst::slice::{TryIntoArrayError, try_into_array};
///
/// const _: Result<&[u8; 4], TryIntoArrayError> = {
///     try_into_array!(&[1, 2, 3, 4], 4)
/// };
/// ```
///
/// ```compile_fail
/// use konst::slice::{TryIntoArrayError, try_into_array};
///
/// const _: Result<&[u8; 4], TryIntoArrayError> = {
///     try_into_array!(&[1, 2, 3, 4])
/// };
/// ```
///
#[cfg(not(feature = "rust_1_51"))]
pub struct SliceIntoArrayNoConstGenerics;

///
/// ```
/// use konst::slice::{TryIntoArrayError, try_into_array};
///
/// const _: Result<&[&u8; 1], TryIntoArrayError> = {
///     try_into_array!(&[&10], 1)
/// };
/// ```
///
/// ```compile_fail
/// use konst::slice::{TryIntoArrayError, try_into_array};
///
/// const _: Result<&[&u8; 1], TryIntoArrayError> = {
///     let foo = 10;
///     try_into_array!(&[&foo], 1)
/// };
/// ```
///
/// ```compile_fail
/// use konst::slice::{TryIntoArrayError, try_into_array};
///
/// const _: Result<&[&u8; 1], TryIntoArrayError> = {
///     let arr: [&'static u8; 1] = [&10];
///     try_into_array!(&arr, 1)
/// };
/// ```
///
pub struct SliceIntoArrayLifetimesExplicitLen;
