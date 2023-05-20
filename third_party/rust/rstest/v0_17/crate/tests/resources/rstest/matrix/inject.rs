use rstest::*;
use actix_rt;
use std::future::Future;

#[rstest(
    first => [1, 2], 
    second => [2, 1],
)]
#[test]
fn sync(first: u32, second: u32) { assert_eq!(2, first * second); }

#[rstest(
    first => [async { 1 }, async { 2 }], 
    second => [2, 1],
)]
#[actix_rt::test]
async fn fn_async(first: impl Future<Output=u32>, second: u32) { assert_eq!(2, first.await * second); }
