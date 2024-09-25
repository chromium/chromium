use rstest::*;
use std::time::Duration;

fn ms(ms: u32) -> Duration {
    Duration::from_millis(ms.into())
}

async fn delayed_sum(a: u32, b: u32,delay: Duration) -> u32 {
    async_std::task::sleep(delay).await;
    a + b
}

#[rstest]
#[timeout(ms(80))]
async fn single_pass() {
    assert_eq!(4, delayed_sum(2, 2, ms(10)).await);
}