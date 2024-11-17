use rstest::*;

use sqlx::SqlitePool;

struct FixtureStruct {}

#[fixture]
fn my_fixture() -> FixtureStruct {
    FixtureStruct {}
}

#[rstest]
#[sqlx::test]
async fn test_db(my_fixture: FixtureStruct, #[ignore] pool: SqlitePool) {
    assert!(true);
}
