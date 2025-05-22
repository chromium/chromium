## What's Changed in 0.0.7
* Bump ixdtf and complete changes for update by @nekevss in [#299](https://github.com/boa-dev/temporal/pull/299)
* A few more changes to the readme by @nekevss in [#297](https://github.com/boa-dev/temporal/pull/297)
* Implement a builder API for Now by @nekevss in [#296](https://github.com/boa-dev/temporal/pull/296)
* Some docs cleanup and updates by @nekevss in [#294](https://github.com/boa-dev/temporal/pull/294)
* Add `PlainMonthDay` method + clean up by @nekevss in [#284](https://github.com/boa-dev/temporal/pull/284)
* Add Eq, Ord impls on FiniteF64 by @sffc in [#187](https://github.com/boa-dev/temporal/pull/187)
* Allow parsers to accept unvalidated UTF8 by @HalidOdat in [#295](https://github.com/boa-dev/temporal/pull/295)
* Bump to icu_calendar 2.0 by @nekevss in [#292](https://github.com/boa-dev/temporal/pull/292)
* Add ISO specific constructors to builtins by @nekevss in [#263](https://github.com/boa-dev/temporal/pull/263)
* Rename the provider crate by @nekevss in [#289](https://github.com/boa-dev/temporal/pull/289)
* Expose equals and compare over FFI by @Magnus-Fjeldstad in [#269](https://github.com/boa-dev/temporal/pull/269)
* Impl round_with_provider for ZonedDateTIme by @sebastianjacmatt in [#278](https://github.com/boa-dev/temporal/pull/278)
* Add some compiled_data FFI APIs by @Manishearth in [#273](https://github.com/boa-dev/temporal/pull/273)
* Add string conversion functions for Temporal types by @Manishearth in [#276](https://github.com/boa-dev/temporal/pull/276)
* Make sure temporal_capi can be built no_std by @Manishearth in [#281](https://github.com/boa-dev/temporal/pull/281)
* Make iana-time-zone dep optional by @Manishearth in [#279](https://github.com/boa-dev/temporal/pull/279)
* Implementation of ZonedDateTime.prototype.with by @lockels in [#267](https://github.com/boa-dev/temporal/pull/267)
* Update Duration's inner representation from floating point to integers. by @nekevss in [#268](https://github.com/boa-dev/temporal/pull/268)
* Add all-features = true to docs.rs metadata by @Manishearth in [#271](https://github.com/boa-dev/temporal/pull/271)
* Fix instant math in capi by @Manishearth in [#270](https://github.com/boa-dev/temporal/pull/270)
* Remove the Temporal prefix from Unit, RoundingMode, and UnsignedRoundingMode by @nekevss in [#254](https://github.com/boa-dev/temporal/pull/254)
* Since until by @sebastianjacmatt in [#259](https://github.com/boa-dev/temporal/pull/259)
* Implementation of toZonedDateTimeISO for Instant by @lockels in [#258](https://github.com/boa-dev/temporal/pull/258)
* Implement toZonedDateTime in PlainDate by @JohannesHelleve in [#192](https://github.com/boa-dev/temporal/pull/192)
* Add intro documentation to ZonedDateTime and PlainDateTime by @nekevss in [#253](https://github.com/boa-dev/temporal/pull/253)
* Implement IANA normalizer baked data provider by @nekevss in [#251](https://github.com/boa-dev/temporal/pull/251)

## New Contributors
* @HalidOdat made their first contribution in [#295](https://github.com/boa-dev/temporal/pull/295)
* @JohannesHelleve made their first contribution in [#192](https://github.com/boa-dev/temporal/pull/192)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.6...0.0.7

## What's Changed in 0.0.6
* Rename methods on `Now` and add a test by @nekevss in [#243](https://github.com/boa-dev/temporal/pull/243)
* Add licenses to `temporal_capi` and `temporal_rs` for publish by @nekevss in [#247](https://github.com/boa-dev/temporal/pull/247)
* Add with to PlainYearMonth by @sebastianjacmatt in [#231](https://github.com/boa-dev/temporal/pull/231)
* initial implementation of ToZonedDateTime, ToPlainDate, ToPlainTime by @lockels in [#244](https://github.com/boa-dev/temporal/pull/244)
* Initial implementation of Duration.prototype.total by @lockels in [#241](https://github.com/boa-dev/temporal/pull/241)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.5...0.0.6

## What's Changed in 0.0.5
* Prepare temporal_capi for publish by @jedel1043 in [#238](https://github.com/boa-dev/temporal/pull/238)
* Adjustments to `toPlainYearMonth` and `toPlainMonthDay` methods on PlainDate by @nekevss in [#237](https://github.com/boa-dev/temporal/pull/237)
* Missed an unwrap in the README.md example by @nekevss in [#236](https://github.com/boa-dev/temporal/pull/236)
* Clean up API ergonomics for calendar methods by @nekevss in [#235](https://github.com/boa-dev/temporal/pull/235)
* Add various updates to docs by @nekevss in [#234](https://github.com/boa-dev/temporal/pull/234)
* Reject datetime when fraction digits are too large by @nekevss in [#229](https://github.com/boa-dev/temporal/pull/229)
* Fix not adjusting fraction for duration unit by @nekevss in [#228](https://github.com/boa-dev/temporal/pull/228)
* Fixes for `EpochNanosecond`s and `Offset` parsing by @nekevss in [#223](https://github.com/boa-dev/temporal/pull/223)
* Fix bugs around validating diffing units by @nekevss in [#225](https://github.com/boa-dev/temporal/pull/225)
* Add `UtcOffset` struct for `PartialZonedDateTime` by @nekevss in [#207](https://github.com/boa-dev/temporal/pull/207)
* Check in bindings, add CI for keeping them up to date by @Manishearth in [#220](https://github.com/boa-dev/temporal/pull/220)
* Fix `Instant::epoch_milliseconds` for values before Epoch by @nekevss in [#221](https://github.com/boa-dev/temporal/pull/221)
* Update ixdtf to 0.4.0 by @Manishearth in [#219](https://github.com/boa-dev/temporal/pull/219)
* Update icu4x to 2.0.0-beta2 by @Manishearth in [#218](https://github.com/boa-dev/temporal/pull/218)
* Add `GetNamedTimeZoneTransition` method to TimeZoneProvider trait by @nekevss in [#203](https://github.com/boa-dev/temporal/pull/203)
* Implement posix resolution for month-week-day by @jedel1043 in [#214](https://github.com/boa-dev/temporal/pull/214)
* implement utility methods on partial structs by @nekevss in [#206](https://github.com/boa-dev/temporal/pull/206)
* Test all combinations of features by @jedel1043 in [#212](https://github.com/boa-dev/temporal/pull/212)
* Reject non-iso calendar in `YearMonth::from_str` and `MonthDay::from_str` by @nekevss in [#211](https://github.com/boa-dev/temporal/pull/211)
* Fix not validating `MonthCode` in ISO path by @nekevss in [#210](https://github.com/boa-dev/temporal/pull/210)
* Integrate `MonthCode` into public API and related adjustments by @nekevss in [#208](https://github.com/boa-dev/temporal/pull/208)
* Implement the remaining non-ISO calendar method calls by @nekevss in [#209](https://github.com/boa-dev/temporal/pull/209)
* PlainYearMonth parsing and Calendar field resolution cleanup/fixes by @nekevss in [#205](https://github.com/boa-dev/temporal/pull/205)
* Build out stubs for remaining unimplemented methods by @nekevss in [#202](https://github.com/boa-dev/temporal/pull/202)
* Fix clippy lints by @nekevss in [#201](https://github.com/boa-dev/temporal/pull/201)
* Temporal duration compare by @sffc, @lockels, @Neelzee, @sebastianjacmatt, @Magnus-Fjeldstad and @HenrikTennebekk. in [#186](https://github.com/boa-dev/temporal/pull/186)
* Fix issues with `from_partial` method implementations by @nekevss in [#200](https://github.com/boa-dev/temporal/pull/200)
* More FFI: Finish Calendar, add Instant by @Manishearth in [#198](https://github.com/boa-dev/temporal/pull/198)
* Fix parsing bugs related to UTC Designator usage by @nekevss in [#197](https://github.com/boa-dev/temporal/pull/197)
* Update time parsing to error on dup critical calendars by @nekevss in [#196](https://github.com/boa-dev/temporal/pull/196)
* Update unit group validation to handle rounding options by @nekevss in [#194](https://github.com/boa-dev/temporal/pull/194)
* Fix handling of leap seconds in parsing by @nekevss in [#195](https://github.com/boa-dev/temporal/pull/195)
* Update time zone parsing to include other ixdtf formats by @nekevss in [#193](https://github.com/boa-dev/temporal/pull/193)
* Fix calendar parsing on `from_str` implementation by @nekevss in [#191](https://github.com/boa-dev/temporal/pull/191)
* Cleanup `Now` API in favor of system defined implementations by @nekevss in [#182](https://github.com/boa-dev/temporal/pull/182)
* Reimplement Unit Group with different approach by @nekevss in [#183](https://github.com/boa-dev/temporal/pull/183)
* Implement `ZonedDateTime::offset` and `ZonedDateTime::offset_nanoseconds` by @nekevss in [#185](https://github.com/boa-dev/temporal/pull/185)
* Small API cleanup and a couple dev docs updates by @nekevss in [#190](https://github.com/boa-dev/temporal/pull/190)
* Add Eq and Ord for PlainYearMonth + Eq for PlainMonthDay by @nekevss in [#175](https://github.com/boa-dev/temporal/pull/175)
* More FFI APIs by @Manishearth in [#178](https://github.com/boa-dev/temporal/pull/178)
* Fix the typo that's returning milliseconds instead of microseconds by @nekevss in [#184](https://github.com/boa-dev/temporal/pull/184)
* Add some FFI tests by @Manishearth in [#179](https://github.com/boa-dev/temporal/pull/179)
* Fix Instant parsing by handling order of operations and properly balance `IsoDateTime` by @nekevss in [#174](https://github.com/boa-dev/temporal/pull/174)
* Add some testing / debugging docs by @nekevss in [#176](https://github.com/boa-dev/temporal/pull/176)
* Fix logic on asserting is_time_duration by @nekevss in [#177](https://github.com/boa-dev/temporal/pull/177)
* Rework library restructure to remove wrapper types by @nekevss in [#181](https://github.com/boa-dev/temporal/pull/181)
* Restructure project to separate core provider APIs from non-provider APIs by @nekevss in [#169](https://github.com/boa-dev/temporal/pull/169)
* Fix `Duration` parsing not returning a range error. by @nekevss in [#173](https://github.com/boa-dev/temporal/pull/173)
* Set up basic diplomat workflow by @Manishearth in [#163](https://github.com/boa-dev/temporal/pull/163)
* Implement Neri-Schneider calculations by @nekevss in [#147](https://github.com/boa-dev/temporal/pull/147)
* Remove `UnitGroup` addition by @nekevss in [#171](https://github.com/boa-dev/temporal/pull/171)
* Implement `ZonedDateTime::since` and `ZonedDateTime::until` by @nekevss in [#170](https://github.com/boa-dev/temporal/pull/170)
* Implement to_string functionality and methods for `Duration`, `PlainYearMonth`, and `PlainMonthDay` by @nekevss in [#164](https://github.com/boa-dev/temporal/pull/164)
* API cleanup, visibility updates, and tech debt cleanup by @nekevss in [#168](https://github.com/boa-dev/temporal/pull/168)
* Add to_string functionality for timezone identifer by @nekevss in [#161](https://github.com/boa-dev/temporal/pull/161)
* Bug fixes to address test failures + removing unused API by @nekevss in [#162](https://github.com/boa-dev/temporal/pull/162)
* Implement correct resolving of `getStartOfDay` by @jedel1043 in [#159](https://github.com/boa-dev/temporal/pull/159)
* Add an MSRV check to CI by @nekevss in [#158](https://github.com/boa-dev/temporal/pull/158)
* Fixing panics in test262 when running in debug mode by @nekevss in [#157](https://github.com/boa-dev/temporal/pull/157)
* Fix edge case for disambiguating `ZonedDateTime`s on DSTs skipping midnight by @jedel1043 in [#156](https://github.com/boa-dev/temporal/pull/156)
* Extend implementation of `to_ixdtf_string` to more types by @nekevss in [#155](https://github.com/boa-dev/temporal/pull/155)
* Implement support for `to_string` and implement `PlainDate::to_string` by @nekevss in [#153](https://github.com/boa-dev/temporal/pull/153)
* Fix RoundingMode::truncation to UnsignedRoundingMode mapping by @nekevss in [#146](https://github.com/boa-dev/temporal/pull/146)
* Add validation logic to `from_diff_settings` by @nekevss in [#144](https://github.com/boa-dev/temporal/pull/144)
* Build out the rest of the Now methods by @nekevss in [#145](https://github.com/boa-dev/temporal/pull/145)
* Adjust `RelativeTo` according to specification and implementation by @nekevss in [#140](https://github.com/boa-dev/temporal/pull/140)
* Complete some general cleanup of `temporal_rs` by @nekevss in [#138](https://github.com/boa-dev/temporal/pull/138)
* Add ZonedDateTime functionality to `Duration::round` by @nekevss in [#134](https://github.com/boa-dev/temporal/pull/134)
* Update try_new, new, and new_with_overflow integer type by @nekevss in [#137](https://github.com/boa-dev/temporal/pull/137)
* Fix `Calendar::from_str` to be case-insensitive by @nekevss in [#135](https://github.com/boa-dev/temporal/pull/135)
* Add ToIntegerWithTruncation, ToPositiveIntegerWithTruncation, and ToIntegerIfIntegral methods to `FiniteF64` by @nekevss in [#131](https://github.com/boa-dev/temporal/pull/131)
* Add to-x methods and with-x methods to ZonedDateTime by @nekevss in [#129](https://github.com/boa-dev/temporal/pull/129)
* Fix `epoch_time_to_epoch_year` date equation bug related to `BalanceISODate` by @nekevss in [#132](https://github.com/boa-dev/temporal/pull/132)
* Bump dependencies for `ixdtf`, `tzif`, `icu_calendar`, and `tinystr`. by @nekevss in [#133](https://github.com/boa-dev/temporal/pull/133)
* Fix bug introduced by `EpochNanoseconds` + adjust tests to catch better by @nekevss in [#128](https://github.com/boa-dev/temporal/pull/128)
* Remove epochSeconds and epochMicroseconds + adjust epochMillseconds by @nekevss in [#127](https://github.com/boa-dev/temporal/pull/127)
* Adjust compilation configuration of tzdb to target_family from target_os by @nekevss in [#125](https://github.com/boa-dev/temporal/pull/125)
* Migrate repo to workspace by @jedel1043 in [#126](https://github.com/boa-dev/temporal/pull/126)
* Add now feature flag by @nekevss in [#123](https://github.com/boa-dev/temporal/pull/123)
* Add  `ZonedDateTime` calendar accessor methods by @nekevss in [#117](https://github.com/boa-dev/temporal/pull/117)
* Implement  `PartialZonedDateTime` and `from_partial` and `from_str` for `ZonedDateTime` by @nekevss in [#115](https://github.com/boa-dev/temporal/pull/115)
* Update CHANGELOG for v0.0.4 release by @nekevss in [#124](https://github.com/boa-dev/temporal/pull/124)
* Patch the now test per matrix discussion by @nekevss in [#121](https://github.com/boa-dev/temporal/pull/121)

## New Contributors
* @sffc made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @lockels made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @Neelzee made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @sebastianjacmatt made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @Magnus-Fjeldstad made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @HenrikTennebekk made their first contribution in [#186](https://github.com/boa-dev/temporal/pull/186)
* @cassioneri made their first contribution in [#147](https://github.com/boa-dev/temporal/pull/147)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.4...0.0.5

## What's Changed in v0.0.4

* bump release by @jasonwilliams in [#120](https://github.com/boa-dev/temporal/pull/120)
* Add an `EpochNanosecond` new type by @nekevss in [#116](https://github.com/boa-dev/temporal/pull/116)
* Migrate to `web_time::SystemTime` for `wasm32-unknown-unknown` targets by @nekevss in [#118](https://github.com/boa-dev/temporal/pull/118)
* Bug fixes and more implementation by @jasonwilliams in [#110](https://github.com/boa-dev/temporal/pull/110)
* Some `Error` optimizations by @CrazyboyQCD in [#112](https://github.com/boa-dev/temporal/pull/112)
* Add `from_partial` methods to `PlainTime`, `PlainDate`, and `PlainDateTime` by @nekevss in [#106](https://github.com/boa-dev/temporal/pull/106)
* Implement `ZonedDateTime`'s add and subtract methods by @nekevss in [#102](https://github.com/boa-dev/temporal/pull/102)
* Add matrix links to README and some layout adjustments by @nekevss in [#108](https://github.com/boa-dev/temporal/pull/108)
* Stub out `tzdb` support for Windows and POSIX tz string by @nekevss in [#100](https://github.com/boa-dev/temporal/pull/100)
* Stub out tzdb support to unblock `Now` and `ZonedDateTime` by @nekevss in [#99](https://github.com/boa-dev/temporal/pull/99)
* Remove num-bigint dependency and rely on primitives by @nekevss in [#103](https://github.com/boa-dev/temporal/pull/103)
* Move to no_std by @Manishearth in [#101](https://github.com/boa-dev/temporal/pull/101)
* General API cleanup and adjustments by @nekevss in [#97](https://github.com/boa-dev/temporal/pull/97)
* Update README.md by @jasonwilliams in [#96](https://github.com/boa-dev/temporal/pull/96)
* Refactor `TemporalFields` into `CalendarFields` by @nekevss in [#95](https://github.com/boa-dev/temporal/pull/95)
* Patch for partial records by @nekevss in [#94](https://github.com/boa-dev/temporal/pull/94)
* Add `PartialTime` and `PartialDateTime` with corresponding `with` methods. by @nekevss in [#92](https://github.com/boa-dev/temporal/pull/92)
* Implement `MonthCode`, `PartialDate`, and `Date::with` by @nekevss in [#89](https://github.com/boa-dev/temporal/pull/89)
* Add is empty for partialDuration by @jasonwilliams in [#90](https://github.com/boa-dev/temporal/pull/90)
* Fix lints for rustc 1.80.0 by @jedel1043 in [#91](https://github.com/boa-dev/temporal/pull/91)
* adding methods for yearMonth and MonthDay by @jasonwilliams in [#44](https://github.com/boa-dev/temporal/pull/44)
* Implement `DateTime` round method by @nekevss in [#88](https://github.com/boa-dev/temporal/pull/88)
* Update `Duration` types to use a `FiniteF64` instead of `f64` primitive. by @nekevss in [#86](https://github.com/boa-dev/temporal/pull/86)
* Refactor `TemporalFields` interface and add `FieldsKey` enum by @nekevss in [#87](https://github.com/boa-dev/temporal/pull/87)
* Updates to instant and its methods by @nekevss in [#85](https://github.com/boa-dev/temporal/pull/85)
* Implement compare functionality and some more traits by @nekevss in [#82](https://github.com/boa-dev/temporal/pull/82)
* Implement `DateTime` diffing methods `Until` and `Since` by @nekevss in [#83](https://github.com/boa-dev/temporal/pull/83)
* Add `with_*` methods to `Date` and `DateTime` by @nekevss in [#84](https://github.com/boa-dev/temporal/pull/84)
* Add some missing trait implementations by @nekevss in [#81](https://github.com/boa-dev/temporal/pull/81)
* chore(dependabot): bump zerovec-derive from 0.10.2 to 0.10.3 by @dependabot[bot] in [#80](https://github.com/boa-dev/temporal/pull/80)
* Add prefix option to commit-message by @nekevss in [#79](https://github.com/boa-dev/temporal/pull/79)
* Add commit-message prefix to dependabot by @nekevss in [#77](https://github.com/boa-dev/temporal/pull/77)
* Bump zerovec from 0.10.2 to 0.10.4 by @dependabot[bot] in [#78](https://github.com/boa-dev/temporal/pull/78)

## New Contributors
* @jasonwilliams made their first contribution in [#120](https://github.com/boa-dev/temporal/pull/120)
* @CrazyboyQCD made their first contribution in [#112](https://github.com/boa-dev/temporal/pull/112)
* @Manishearth made their first contribution in [#101](https://github.com/boa-dev/temporal/pull/101)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.3...v0.0.4

# CHANGELOG

## What's Changed in v0.0.3

* Implement add and subtract methods for Duration by @nekevss in [#74](https://github.com/boa-dev/temporal/pull/74)
* Implement PartialEq and Eq for `Calendar`, `Date`, and `DateTime` by @nekevss in [#75](https://github.com/boa-dev/temporal/pull/75)
* Update duration validation and switch asserts to debug-asserts by @nekevss in [#73](https://github.com/boa-dev/temporal/pull/73)
* Update duration rounding to new algorithms by @nekevss in [#65](https://github.com/boa-dev/temporal/pull/65)
* Remove `CalendarProtocol` and `TimeZoneProtocol` by @jedel1043 in [#66](https://github.com/boa-dev/temporal/pull/66)
* Use groups in dependabot updates by @jedel1043 in [#69](https://github.com/boa-dev/temporal/pull/69)
* Ensure parsing throws with unknown critical annotations by @jedel1043 in [#63](https://github.com/boa-dev/temporal/pull/63)
* Reject `IsoDate` when outside the allowed range by @jedel1043 in [#62](https://github.com/boa-dev/temporal/pull/62)
* Avoid overflowing when calling `NormalizedTimeDuration::add_days` by @jedel1043 in [#61](https://github.com/boa-dev/temporal/pull/61)
* Ensure parsing throws when duplicate calendar is critical by @jedel1043 in [#58](https://github.com/boa-dev/temporal/pull/58)
* Fix rounding when the dividend is smaller than the divisor by @jedel1043 in [#57](https://github.com/boa-dev/temporal/pull/57)
* Implement the `toYearMonth`, `toMonthDay`, and `toDateTime` for `Date` component by @nekevss in [#56](https://github.com/boa-dev/temporal/pull/56)
* Update increment rounding functionality by @nekevss in [#53](https://github.com/boa-dev/temporal/pull/53)
* Patch `(un)balance_relative` to avoid panicking by @jedel1043 in [#48](https://github.com/boa-dev/temporal/pull/48)
* Cleanup rounding increment usages with new struct by @jedel1043 in [#54](https://github.com/boa-dev/temporal/pull/54)
* Add struct to encapsulate invariants of rounding increments by @jedel1043 in [#49](https://github.com/boa-dev/temporal/pull/49)
* Migrate parsing to `ixdtf` crate by @nekevss in [#50](https://github.com/boa-dev/temporal/pull/50)
* Fix method call in days_in_month by @nekevss in [#46](https://github.com/boa-dev/temporal/pull/46)
* Implement add & subtract methods for `DateTime` component by @nekevss in [#45](https://github.com/boa-dev/temporal/pull/45)
* Fix panics when no relative_to is supplied to round by @nekevss in [#40](https://github.com/boa-dev/temporal/pull/40)
* Implement Time's until and since methods by @nekevss in [#36](https://github.com/boa-dev/temporal/pull/36)
* Implements `Date`'s `add`, `subtract`, `until`, and `since` methods by @nekevss in [#35](https://github.com/boa-dev/temporal/pull/35)
* Fix clippy lints and bump bitflags version by @nekevss in [#38](https://github.com/boa-dev/temporal/pull/38)

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.2...v0.0.3

## What's Changed in v0.0.2

# [0.0.2 (2024-03-04)](https://github.com/boa-dev/temporal/compare/v0.0.1...v0.0.2)

### Enhancements

* Fix loop in `diff_iso_date` by @nekevss in https://github.com/boa-dev/temporal/pull/31
* Remove unnecessary iterations by @nekevss in https://github.com/boa-dev/temporal/pull/30

**Full Changelog**: https://github.com/boa-dev/temporal/compare/v0.0.1...v0.0.2

# [0.0.1 (2024-02-25)](https://github.com/boa-dev/temporal/commits/v0.0.1)

### Enhancements
* Add blank and negated + small adjustments by @nekevss in https://github.com/boa-dev/temporal/pull/17
* Simplify Temporal APIs by @jedel1043 in https://github.com/boa-dev/temporal/pull/18
* Implement `Duration` normalization - Part 1 by @nekevss in https://github.com/boa-dev/temporal/pull/20
* Duration Normalization - Part 2 by @nekevss in https://github.com/boa-dev/temporal/pull/23
* Add `non_exhaustive` attribute to component structs by @nekevss in https://github.com/boa-dev/temporal/pull/25
* Implement `Duration::round` and some general updates/fixes by @nekevss in https://github.com/boa-dev/temporal/pull/24

### Documentation
* Adding a `docs` directory by @nekevss in https://github.com/boa-dev/temporal/pull/16
* Build out README and CONTRIBUTING docs by @nekevss in https://github.com/boa-dev/temporal/pull/21

### Other Changes
* Port `boa_temporal` to new `temporal` crate by @nekevss in https://github.com/boa-dev/temporal/pull/1
* Add CI and rename license by @jedel1043 in https://github.com/boa-dev/temporal/pull/3
* Create LICENSE-Apache by @jedel1043 in https://github.com/boa-dev/temporal/pull/6
* Setup publish CI by @jedel1043 in https://github.com/boa-dev/temporal/pull/26
* Remove keywords from Cargo.toml by @jedel1043 in https://github.com/boa-dev/temporal/pull/28

## New Contributors
* @nekevss made their first contribution in https://github.com/boa-dev/temporal/pull/1
* @jedel1043 made their first contribution in https://github.com/boa-dev/temporal/pull/3

**Full Changelog**: https://github.com/boa-dev/temporal/commits/v0.0.1