use rstest::*;

#[rstest]
#[awt]
async fn my_async_test(
    #[future] 
    #[values(async { 1 }, async { 2 })] 
    first: u32, 
    #[values(42, 21)] 
    second: u32
) {
    assert_eq!(42, first * second);
}