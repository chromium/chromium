use rstest::*;
use actix_rt;

#[fixture]
fn a() -> u32 {
    42
}

#[rstest]
#[test]
fn sync_case(a: u32) {}

#[rstest]
#[test]
#[should_panic]
fn sync_case_panic(a: u32) { panic!("panic") }

#[rstest]
#[test]
fn sync_case_fail(a: u32) { assert_eq!(2, a); }

#[rstest]
#[test]
fn sync_case_panic_fail(a: u32) { panic!("panic") }

#[rstest]
#[actix_rt::test]
async fn async_case(a: u32) {}

#[rstest]
#[actix_rt::test]
async fn async_case_fail(a: u32) { assert_eq!(2, a); }

#[rstest]
#[actix_rt::test]
#[should_panic]
async fn async_case_panic(a: u32) { panic!("panic") }

#[rstest]
#[actix_rt::test]
async fn async_case_panic_fail(a: u32) { panic!("panic") }