// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// map_keys defines constants for use in CBOR structures. Since the keys for
/// CBOR maps are an enum, doing a lookup without allocating that enum type is
/// a little complex and we end up wanting to define two constants for each
/// map key: the `&str` and a constant for looking the value up in a map.
///
/// This macro takes a comma-separated list where each item looks like
/// `FOO, FOO_KEY = "foo"` and defines `FOO` as a `&str` for "foo", and
/// `FOO_KEY` as a lookup key for "foo".
macro_rules! map_keys {
  ($( $name:ident, $keyname:ident = $value:expr ),*) => {
    $(
    pub(crate) const $name: &str = $value;
    #[allow(dead_code)]
    pub(crate) const $keyname: &dyn MapLookupKey = &MapKeyRef::Str($name) as &dyn MapLookupKey;
    )*
  };
  // This pattern is needed to support trailing commas.
  ($( $name:ident, $keyname:ident = $value:expr ),* ,) => {
    map_keys! { $( $name, $keyname = $value),* }
  };
}
