<a name="v1.3.0"></a>
## v1.3.0 (2020-08-12)


#### Bug Fixes

*   Add custom `impl Default` to support non-Default-able `<T>` types ([b49f4eab](https://github.com/indiv0/lazycell/commit/b49f4eabec49c0a5146ef01017c2506a3c357180))
* **lazycell:**  Fix unsound aliasing in `LazyCell::fill` ([e789ac1a](https://github.com/indiv0/lazycell/commit/e789ac1a99010ad79c2d09c761fec6d67053647d), closes [#98](https://github.com/indiv0/lazycell/issues/98))

#### Features

*   Implement serde support ([e728a0b6](https://github.com/indiv0/lazycell/commit/e728a0b680e607b793a81b5af7bf7f1d2c0eb5e5))

#### Documentation

*   fix typo ([5f5ba9d5](https://github.com/indiv0/lazycell/commit/5f5ba9d5ac3364f8376c0c872c2e5094974385ba))



<a name="v1.2.1"></a>
## v1.2.1 (2018-12-03)


#### Features

*   Implement Clone for LazyCell and AtomicLazyCell ([30fe4a8f](https://github.com/indiv0/lazycell/commit/30fe4a8f568059b3c78ed149a810962a676cb2b2))



<a name="v1.2.0"></a>
## v1.2.0 (2018-09-19)


#### Features

*   add `LazyCell::replace` for infallible access ([a63ffb90](https://github.com/indiv0/lazycell/commit/a63ffb9040a5e0683a9bbf9d3d5ef589f2ca8b7c))



<a name="v1.1.0"></a>
## v1.1.0 (2018-09-10)


#### Documentation

*   add note regarding LazyCell::borrow_mut ([9d634d1f](https://github.com/indiv0/lazycell/commit/9d634d1fd9a21b7aa075d407bedf9fe77ba8b79f))
*   describe mutability more consistently ([b8078029](https://github.com/indiv0/lazycell/commit/b80780294611e92efddcdd33a701b3049ab5c5eb), closes [#78](https://github.com/indiv0/lazycell/issues/78))

#### Improvements

*   add NONE constant for an empty AtomicLazyCell ([31aff0da](https://github.com/indiv0/lazycell/commit/31aff0dacf824841c5f38ef4acf0aa71ec4c36eb), closes [#87](https://github.com/indiv0/lazycell/issues/87))
*   add `LazyCell::borrow_mut_with` and `LazyCell::try_borrow_mut_with` ([fdc6d268](https://github.com/indiv0/lazycell/commit/fdc6d268f0e9a6668768302f45fe2bb4aa9a7c34), closes [#79](https://github.com/indiv0/lazycell/issues/79), [#80](https://github.com/indiv0/lazycell/issues/80))



<a name="v1.0.0"></a>
## v1.0.0 (2018-06-06)


#### Features

*   Add #![no_std] ([e59f6b55](https://github.com/indiv0/lazycell/commit/e59f6b5531e310d3df26b0eb40b1431937f38096))



<a name="0.6.0"></a>
## 0.6.0 (2017-11-25)


#### Bug Fixes

*   fix soundness hole in borrow_with ([d1f46bef](https://github.com/indiv0/lazycell/commit/d1f46bef9d1397570aa9c3e87e18e0d16e6d1585))

#### Features

*   add Default derives ([71bc5088](https://github.com/indiv0/lazycell/commit/71bc50880cd8e20002038197c9b890f5b76ad096))
*   add LazyCell::try_borrow_with ([bffa4028](https://github.com/indiv0/lazycell/commit/bffa402896670b5c78a9ec050d82a58ee98de6fb))
*   add LazyCell::borrow_mut method ([fd419dea](https://github.com/indiv0/lazycell/commit/fd419dea965ff1ad3853f26f37e8d107c6ca096c))

#### Breaking Changes

*   add `T: Send` for `AtomicLazyCell` `Sync` impl ([668bb2fa](https://github.com/indiv0/lazycell/commit/668bb2fa974fd6707c4c7edad292c76a9017d74d), closes [#67](https://github.com/indiv0/lazycell/issues/67))

#### Improvements

*   add `T: Send` for `AtomicLazyCell` `Sync` impl ([668bb2fa](https://github.com/indiv0/lazycell/commit/668bb2fa974fd6707c4c7edad292c76a9017d74d), closes [#67](https://github.com/indiv0/lazycell/issues/67))



<a name="v0.5.1"></a>
## v0.5.1 (2017-03-24)


#### Documentation

*   fix missing backticks ([44bafaaf](https://github.com/indiv0/lazycell/commit/44bafaaf93a91641261f58ee38adadcd4af6458e))

#### Improvements

*   derive `Debug` impls ([9da0a5a2](https://github.com/indiv0/lazycell/commit/9da0a5a2ffac1fef03ef02851c2c89d26d67d225))

#### Features

*   Add get method for Copy types ([dc8f8209](https://github.com/indiv0/lazycell/commit/dc8f8209888b6eba6d18717eba6a22614629b997))



<a name="v0.5.0"></a>
## v0.5.0 (2016-12-08)


#### Features

*   add borrow_with to LazyCell ([a15efa35](https://github.com/indiv0/lazycell/commit/a15efa359ea5a31a66ba57fc5b25f90c87b4b0dd))



<a name="v0.4.0"></a>
##  (2016-08-17)


#### Breaking Changes

* **LazyCell:**  return Err(value) on full cell ([68f3415d](https://github.com/indiv0/lazycell/commit/68f3415dd5d6a66ba047a133b7028ebe4f1c5070), breaks [#](https://github.com/indiv0/lazycell/issues/))

#### Improvements

* **LazyCell:**  return Err(value) on full cell ([68f3415d](https://github.com/indiv0/lazycell/commit/68f3415dd5d6a66ba047a133b7028ebe4f1c5070), breaks [#](https://github.com/indiv0/lazycell/issues/))



<a name="v0.3.0"></a>
##  (2016-08-16)


#### Features

*   add AtomicLazyCell which is thread-safe ([85afbd36](https://github.com/indiv0/lazycell/commit/85afbd36d8a148e14cc53654b39ddb523980124d))

#### Improvements

*   Use UnsafeCell instead of RefCell ([3347a8e9](https://github.com/indiv0/lazycell/commit/3347a8e97d2215a47e25c1e2fc953e8052ad8eb6))



<a name="v0.2.1"></a>
##  (2016-04-18)


#### Documentation

*   put types in between backticks ([607cf939](https://github.com/indiv0/lazycell/commit/607cf939b05e35001ba3070ec7a0b17b064e7be1))



<a name="v0.2.0"></a>
## v0.2.0 (2016-03-28)


#### Features

* **lazycell:**
  *  add tests for `LazyCell` struct ([38f1313d](https://github.com/indiv0/lazycell/commit/38f1313d98542ca8c98b424edfa9ba9c3975f99e), closes [#30](https://github.com/indiv0/lazycell/issues/30))
  *  remove unnecessary `Default` impl ([68c16d2d](https://github.com/indiv0/lazycell/commit/68c16d2df4e9d13d5298162c06edf918246fd758))

#### Documentation

* **CHANGELOG:**  removed unnecessary sections ([1cc0555d](https://github.com/indiv0/lazycell/commit/1cc0555d875898a01b0832ff967aed6b40e720eb))
* **README:**  add link to documentation ([c8dc33f0](https://github.com/indiv0/lazycell/commit/c8dc33f01f2c0dc187f59ee53a2b73081053012b), closes [#13](https://github.com/indiv0/lazycell/issues/13))



<a name="v0.1.0"></a>
## v0.1.0 (2016-03-16)


#### Features

* **lib.rs:**  implement Default trait for LazyCell ([150a6304](https://github.com/indiv0/LazyCell/commit/150a6304a230ee1de8424e49c447ec1b2d6578ce))



<a name="v0.0.1"></a>
## v0.0.1 (2016-03-16)


#### Bug Fixes

* **Cargo.toml:**  loosen restrictions on Clippy version ([84dd8f96](https://github.com/indiv0/LazyCell/commit/84dd8f960000294f9dad47d776a41b98ed812981))

#### Features

*   add initial implementation ([4b39764a](https://github.com/indiv0/LazyCell/commit/4b39764a575bcb701dbd8047b966f72720fd18a4))
*   add initial commit ([a80407a9](https://github.com/indiv0/LazyCell/commit/a80407a907ef7c9401f120104663172f6965521a))



