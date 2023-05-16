## 0.4.0 2021-07-11

* Split the `mac` tests into `mac` and `mac_with_iv` to better
  match the Wycheproof schema.
* Some macro helper improvements

## 0.3.0 2021-07-04

* `TestSet::algorithm` is now an enumeration
* `TestSet::header` is now a `String` instead of a `Vec<String>`
* Add many macros to reduce code duplication

## 0.2.0 2021-07-01

* Add `TestName` enums to allow better typechecking
* Split up into several modules; now everything is of the form
  `wycheproof::foo::{TestName, TestSet, TestGroup, Test, TestFlag}`
* Some data was inadvertantly not `pub`

## 0.1.0 2021-06-26

* First release

