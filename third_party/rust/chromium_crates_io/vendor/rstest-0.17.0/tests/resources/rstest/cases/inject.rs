use rstest::*;
use actix_rt;
use std::future::Future;

#[rstest(expected, value,
    case::pass(42, 42),
    #[should_panic]
    case::panic(41, 42),
    case::fail(1, 42)
)]
#[test]
fn sync(expected: u32, value: u32) { assert_eq!(expected, value); }

#[rstest(expected, value,
    case::pass(42, async { 42 }),
    #[should_panic]
    case::panic(41, async { 42 }),
    case::fail(1, async { 42 })
)]
#[actix_rt::test]
async fn fn_async(expected: u32, value: impl Future<Output=u32>) { assert_eq!(expected, value.await); }
