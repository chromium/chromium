#![cfg(all(feature = "std", feature = "derive"))]

extern crate std;

use std::collections::BTreeMap;

#[derive(bincode::Decode, bincode::Encode)]
struct AllTypes(BTreeMap<u8, AllTypes>);

#[test]
fn test_issue_467() {
    let _result: Result<(AllTypes, _), _> =
        bincode::decode_from_slice(&[], bincode::config::standard().with_limit::<1024>());
}
