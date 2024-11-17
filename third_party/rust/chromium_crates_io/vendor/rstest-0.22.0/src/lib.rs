#![allow(clippy::test_attr_in_doctest)]
//! This crate will help you to write simpler tests by leveraging a software testing concept called
//! [test fixtures](https://en.wikipedia.org/wiki/Test_fixture#Software). A fixture is something
//! that you can use in your tests to encapsulate a test's dependencies.
//!
//! The general idea is to have smaller tests that only describe the thing you're testing while you
//! hide the auxiliary utilities your tests make use of somewhere else.
//! For instance, if you have an application that has many tests with users, shopping baskets, and
//! products, you'd have to create a user, a shopping basket, and product every single time in
//! every test which becomes unwieldy quickly. In order to cut down on that repetition, you can
//! instead use fixtures to declare that you need those objects for your function and the fixtures
//! will take care of creating those by themselves. Focus on the important stuff in your tests!
//!
//! In `rstest` a fixture is a function that can return any kind of valid Rust type. This
//! effectively means that your fixtures are not limited by the kind of data they can return.
//! A test can consume an arbitrary number of fixtures at the same time.
//!
//! ## What
//!
//! The `rstest` crate defines the following procedural macros:
//!
//! - [`[rstest]`](macro@rstest): Declare that a test or a group of tests that may take
//!   [fixtures](attr.rstest.html#injecting-fixtures),
//!   [input table](attr.rstest.html#test-parametrized-cases) or
//!   [list of values](attr.rstest.html#values-lists).
//! - [`[fixture]`](macro@fixture): To mark a function as a fixture.
//!
//! ## Why
//!
//! Very often in Rust we write tests like this
//!
//! ```
//! #[test]
//! fn should_process_two_users() {
//!     let mut repository = create_repository();
//!     repository.add("Bob", 21);
//!     repository.add("Alice", 22);
//!
//!     let processor = string_processor();
//!     processor.send_all(&repository, "Good Morning");
//!
//!     assert_eq!(2, processor.output.find("Good Morning").count());
//!     assert!(processor.output.contains("Bob"));
//!     assert!(processor.output.contains("Alice"));
//! }
//! ```
//!
//! By making use of [`[rstest]`](macro@rstest) we can isolate the dependencies `empty_repository` and
//! `string_processor` by passing them as fixtures:
//!
//! ```
//! # use rstest::*;
//! #[rstest]
//! fn should_process_two_users(mut empty_repository: impl Repository,
//!                             string_processor: FakeProcessor) {
//!     empty_repository.add("Bob", 21);
//!     empty_repository.add("Alice", 22);
//!
//!     string_processor.send_all("Good Morning");
//!
//!     assert_eq!(2, string_processor.output.find("Good Morning").count());
//!     assert!(string_processor.output.contains("Bob"));
//!     assert!(string_processor.output.contains("Alice"));
//! }
//! ```
//!
//! ... or if you use `"Alice"` and `"Bob"` in other tests, you can isolate `alice_and_bob` fixture
//! and use it directly:
//!
//! ```
//! # use rstest::*;
//! # trait Repository { fn add(&mut self, name: &str, age: u8); }
//! # struct Rep;
//! # impl Repository for Rep { fn add(&mut self, name: &str, age: u8) {} }
//! # #[fixture]
//! # fn empty_repository() -> Rep {
//! #     Rep
//! # }
//! #[fixture]
//! fn alice_and_bob(mut empty_repository: impl Repository) -> impl Repository {
//!     empty_repository.add("Bob", 21);
//!     empty_repository.add("Alice", 22);
//!     empty_repository
//! }
//!
//! #[rstest]
//! fn should_process_two_users(alice_and_bob: impl Repository,
//!                             string_processor: FakeProcessor) {
//!     string_processor.send_all("Good Morning");
//!
//!     assert_eq!(2, string_processor.output.find("Good Morning").count());
//!     assert!(string_processor.output.contains("Bob"));
//!     assert!(string_processor.output.contains("Alice"));
//! }
//! ```
//! ### Features
//!
//! - `async-timeout`: `timeout` for `async` tests (Default enabled)
//! - `crate-name`: Import `rstest` package with different name (Default enabled)
//!
//! ## Injecting fixtures as function arguments
//!
//! `rstest` functions can receive fixtures by using them as input arguments.
//! A function decorated with [`[rstest]`](attr.rstest.html#injecting-fixtures)
//! will resolve each argument name by call the fixture function.
//! Fixtures should be annotated with the [`[fixture]`](macro@fixture) attribute.
//!
//! Fixtures will be resolved like function calls by following the standard resolution rules.
//! Therefore, an identically named fixture can be use in different context.
//!
//! ```
//! # use rstest::*;
//! # trait Repository { }
//! # #[derive(Default)]
//! # struct DataSet {}
//! # impl Repository for DataSet { }
//! mod empty_cases {
//! # use rstest::*;
//! # trait Repository { }
//! # #[derive(Default)]
//! # struct DataSet {}
//! # impl Repository for DataSet { }
//!     use super::*;
//!
//!     #[fixture]
//!     fn repository() -> impl Repository {
//!         DataSet::default()
//!     }
//!
//!     #[rstest]
//!     fn should_do_nothing(repository: impl Repository) {
//!         //.. test impl ..
//!     }
//! }
//!
//! mod non_trivial_case {
//! # use rstest::*;
//! # trait Repository { }
//! # #[derive(Default)]
//! # struct DataSet {}
//! # impl Repository for DataSet { }
//!     use super::*;
//!
//!     #[fixture]
//!     fn repository() -> impl Repository {
//!         let mut ds = DataSet::default();
//!         // Fill your dataset with interesting case
//!         ds
//!     }
//!
//!     #[rstest]
//!     fn should_notify_all_entries(repository: impl Repository) {
//!         //.. test impl ..
//!     }
//! }
//!
//! ```
//!
//! Last but not least, fixtures can be injected like we saw in `alice_and_bob` example.
//!
//! ## Creating parametrized tests
//!
//! You can use also [`[rstest]`](attr.rstest.html#test-parametrized-cases) to create
//! simple table-based tests. Let's see the classic Fibonacci example:
//!
//! ```
//! use rstest::rstest;
//!
//! #[rstest]
//! #[case(0, 0)]
//! #[case(1, 1)]
//! #[case(2, 1)]
//! #[case(3, 2)]
//! #[case(4, 3)]
//! #[case(5, 5)]
//! #[case(6, 8)]
//! fn fibonacci_test(#[case] input: u32,#[case] expected: u32) {
//!     assert_eq!(expected, fibonacci(input))
//! }
//!
//! fn fibonacci(input: u32) -> u32 {
//!     match input {
//!         0 => 0,
//!         1 => 1,
//!         n => fibonacci(n - 2) + fibonacci(n - 1)
//!     }
//! }
//! ```
//! This will generate a bunch of tests, one for every `#[case(a, b)]`.
//!
//! ## Creating a test for each combinations of given values
//!
//! In some cases you need to test your code for each combinations of some input values. In this
//! cases [`[rstest]`](attr.rstest.html#values-lists) give you the ability to define a list
//! of values (rust expressions) to use for an arguments.
//!
//! ```
//! # use rstest::rstest;
//! # #[derive(PartialEq, Debug)]
//! # enum State { Init, Start, Processing, Terminated }
//! # #[derive(PartialEq, Debug)]
//! # enum Event { Error, Fatal }
//! # impl State { fn process(self, event: Event) -> Self { self } }
//!
//! #[rstest]
//! fn should_terminate(
//!     #[values(State::Init, State::Start, State::Processing)]
//!     state: State,
//!     #[values(Event::Error, Event::Fatal)]
//!     event: Event
//! ) {
//!     assert_eq!(State::Terminated, state.process(event))
//! }
//! ```
//!
//! This will generate a test for each combination of `state` and `event`.
//!
//! ## Magic Conversion
//!
//! If you need a value where its type implement `FromStr()` trait you
//! can use a literal string to build it.
//!
//! ```
//! # use rstest::rstest;
//! # use std::net::SocketAddr;
//! #[rstest]
//! #[case("1.2.3.4:8080", 8080)]
//! #[case("127.0.0.1:9000", 9000)]
//! fn check_port(#[case] addr: SocketAddr, #[case] expected: u16) {
//!     assert_eq!(expected, addr.port());
//! }
//! ```
//! You can use this feature also in value list and in fixture default value.
//!
//! # Optional features
//!
//! `rstest` Enable all features by default. You can disable them if you need to
//! speed up compilation.
//!
//! - **`async-timeout`** *(enabled by default)* — Implement timeout for async
//!   tests.
//!
//! # Rust version compatibility
//!
//! The minimum supported Rust version is 1.67.1.
//!

#[doc(hidden)]
pub mod magic_conversion;
#[doc(hidden)]
pub mod timeout;

/// Define a fixture that you can use in all `rstest`'s test arguments. You should just mark your
/// function as `#[fixture]` and then use it as a test's argument. Fixture functions can also
/// use other fixtures.
///
/// Let's see a trivial example:
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn twenty_one() -> i32 { 21 }
///
/// #[fixture]
/// fn two() -> i32 { 2 }
///
/// #[fixture]
/// fn injected(twenty_one: i32, two: i32) -> i32 { twenty_one * two }
///
/// #[rstest]
/// fn the_test(injected: i32) {
///     assert_eq!(42, injected)
/// }
/// ```
///
/// If the fixture function is an [`async` function](#async) your fixture become an `async`
/// fixture.
///
/// # Default values
///
/// If you need to define argument default value you can use `#[default(expression)]`
/// argument's attribute:
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn injected(
///     #[default(21)]
///     twenty_one: i32,
///     #[default(1 + 1)]
///     two: i32
/// ) -> i32 { twenty_one * two }
///
/// #[rstest]
/// fn the_test(injected: i32) {
///     assert_eq!(42, injected)
/// }
/// ```
/// The `expression` could be any valid rust expression, even an `async` block if you need.
/// Moreover, if the type implements `FromStr` trait you can use a literal string to build it.
///
/// ```
/// # use rstest::*;
/// # use std::net::SocketAddr;
/// # struct DbConnection {}
/// #[fixture]
/// fn db_connection(
///     #[default("127.0.0.1:9000")]
///     addr: SocketAddr
/// ) -> DbConnection {
///     // create connection
/// # DbConnection{}
/// }
/// ```
///
/// # Async
///
/// If you need you can write `async` fixtures to use in your `async` tests. Simply use `async`
/// keyword for your function and the fixture become an `async` fixture.
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// async fn async_fixture() -> i32 { 42 }
///
///
/// #[rstest]
/// async fn the_test(#[future] async_fixture: i32) {
///     assert_eq!(42, async_fixture.await)
/// }
/// ```
/// The `#[future]` argument attribute helps to remove the `impl Future<Output = T>` boilerplate.
/// In this case the macro expands it in:
///
/// ```
/// # use rstest::*;
/// # use std::future::Future;
/// # #[fixture]
/// # async fn async_fixture() -> i32 { 42 }
/// #[rstest]
/// async fn the_test(async_fixture: impl std::future::Future<Output = i32>) {
///     assert_eq!(42, async_fixture.await)
/// }
/// ```
/// If you need, you can use `#[future]` attribute also with an implicit lifetime reference
/// because the macro will replace the implicit lifetime with an explicit one.
///
/// # Rename
///
/// Sometimes you want to have long and descriptive name for your fixture but you prefer to use a much
/// shorter name for argument that represent it in your fixture or test. You can rename the fixture
/// using `#[from(short_name)]` attribute like following example:
///
/// ## Destructuring
///
/// It's possible to destructure the fixture type but, in this case, your're forced to use renaming syntax
/// because it's not possible to guess the fixture name from this syntax:
///
/// ```
/// use rstest::*;
/// #[fixture]
/// fn two_values() -> (u32, u32) { (42, 24) }
///
/// #[rstest]
/// fn the_test(#[from(two_values)] (first, _): (u32, u32)) {
///     assert_eq!(42, first)
/// }
/// ```
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn long_and_boring_descriptive_name() -> i32 { 42 }
///
/// #[rstest]
/// fn the_test(#[from(long_and_boring_descriptive_name)] short: i32) {
///     assert_eq!(42, short)
/// }
/// ```
///
/// This feature can also be useful when you don't want to declare the `use` of a fixture or simple
/// use the fixture's path:
///
/// ```
/// use rstest::*;
///
/// # mod magic_numbers {
/// #     use rstest::*;
/// #     #[fixture]
/// #     pub fn fortytwo() -> i32 { 42 }
/// # }
/// #[rstest]
/// fn the_test(#[from(magic_numbers::fortytwo)] x: i32) {
///     assert_eq!(42, x)
/// }
/// ```
///
/// # `#[once]` Fixture
///
/// Especially in integration tests there are cases where you need a fixture that is called just once
/// for every tests. `rstest` provides `#[once]` attribute for these cases.
///
/// If you mark your fixture with this attribute, then `rstest` will compute a static reference to your
/// fixture result and return this reference to all your tests that need this fixture.
///
/// In follow example all tests share the same reference to the `42` static value.
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// #[once]
/// fn once_fixture() -> i32 { 42 }
///
/// // Take care!!! You need to use a reference to the fixture value
///
/// #[rstest]
/// #[case(1)]
/// #[case(2)]
/// fn cases_tests(once_fixture: &i32, #[case] v: i32) {
///     // Take care!!! You need to use a reference to the fixture value
///     assert_eq!(&42, once_fixture)
/// }
///
/// #[rstest]
/// fn single(once_fixture: &i32) {
///     assert_eq!(&42, once_fixture)
/// }
/// ```
///
/// There are some limitations when you use `#[once]` fixture. `rstest` forbid to use once fixture
/// for:
///
/// - `async` function
/// - Generic function (both with generic types or use `impl` trait)
///
/// Take care that the `#[once]` fixture value will **never be dropped**.
///
/// # Partial Injection
///
/// You can also partially inject fixture dependency using `#[with(v1, v2, ..)]` attribute:
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn base() -> i32 { 1 }
///
/// #[fixture]
/// fn first(base: i32) -> i32 { 1 * base }
///
/// #[fixture]
/// fn second(base: i32) -> i32 { 2 * base }
///
/// #[fixture]
/// fn injected(first: i32, #[with(3)] second: i32) -> i32 { first * second }
///
/// #[rstest]
/// fn the_test(injected: i32) {
///     assert_eq!(-6, injected)
/// }
/// ```
/// Note that injected value can be an arbitrary rust expression. `#[with(v1, ..., vn)]`
/// attribute will inject `v1, ..., vn` expression as fixture arguments: all remaining arguments
/// will be resolved as fixtures.
///
/// Sometimes the return type cannot be inferred so you must define it: For the few times you may
/// need to do it, you can use the `#[default(type)]`, `#[partial_n(type)]` function attribute
/// to define it:
///
/// ```
/// use rstest::*;
/// # use std::fmt::Debug;
///
/// #[fixture]
/// pub fn i() -> u32 {
///     42
/// }
///
/// #[fixture]
/// pub fn j() -> i32 {
///     -42
/// }
///
/// #[fixture]
/// #[default(impl Iterator<Item=(u32, i32)>)]
/// #[partial_1(impl Iterator<Item=(I,i32)>)]
/// pub fn fx<I, J>(i: I, j: J) -> impl Iterator<Item=(I, J)> {
///     std::iter::once((i, j))
/// }
///
/// #[rstest]
/// fn resolve_by_default(mut fx: impl Iterator<Item=(u32, i32)>) {
///     assert_eq!((42, -42), fx.next().unwrap())
/// }
///
/// #[rstest]
/// fn resolve_partial(#[with(42.0)] mut fx: impl Iterator<Item=(f32, i32)>) {
///     assert_eq!((42.0, -42), fx.next().unwrap())
/// }
/// ```
/// `partial_i` is the fixture used when you inject the first `i` arguments in test call.
///
/// # Old _compact_ syntax
///
/// There is also a compact form for all previous features. This will maintained for a long time
/// but for `fixture` I strongly recommend to migrate your code because you'll pay a little
/// verbosity but get back a more readable code.
///
/// Follow the previous examples in old _compact_ syntax.
///
/// ## Default
/// ```
/// # use rstest::*;
/// #[fixture(twenty_one=21, two=2)]
/// fn injected(twenty_one: i32, two: i32) -> i32 { twenty_one * two }
/// ```
///
/// ## Rename
/// ```
/// # use rstest::*;
/// #[fixture]
/// fn long_and_boring_descriptive_name() -> i32 { 42 }
///
/// #[rstest(long_and_boring_descriptive_name as short)]
/// fn the_test(short: i32) {
///     assert_eq!(42, short)
/// }
/// ```
///
/// ## Partial Injection
/// ```
/// # use rstest::*;
/// # #[fixture]
/// # fn base() -> i32 { 1 }
/// #
/// # #[fixture]
/// # fn first(base: i32) -> i32 { 1 * base }
/// #
/// # #[fixture]
/// # fn second(base: i32) -> i32 { 2 * base }
/// #
/// #[fixture(second(-3))]
/// fn injected(first: i32, second: i32) -> i32 { first * second }
/// ```
/// ## Partial Type Injection
/// ```
/// # use rstest::*;
/// # use std::fmt::Debug;
/// #
/// # #[fixture]
/// # pub fn i() -> u32 {
/// #     42
/// # }
/// #
/// # #[fixture]
/// # pub fn j() -> i32 {
/// #     -42
/// # }
/// #
/// #[fixture(::default<impl Iterator<Item=(u32, i32)>>::partial_1<impl Iterator<Item=(I,i32)>>)]
/// pub fn fx<I, J>(i: I, j: J) -> impl Iterator<Item=(I, J)> {
///     std::iter::once((i, j))
/// }
/// ```
pub use rstest_macros::fixture;

/// The attribute that you should use for your tests. Your
/// annotated function's arguments can be
/// [injected](attr.rstest.html#injecting-fixtures) with
/// [`[fixture]`](macro@fixture)s, provided by
/// [parametrized cases](attr.rstest.html#test-parametrized-cases)
/// or by [value lists](attr.rstest.html#values-lists).
///
/// `rstest` attribute can be applied to _any_ function and you can customize its
/// parameters by using function and arguments attributes.
///
/// Your test function can use generics, `impl` or `dyn` and like any kind of rust tests:
///
/// - return results
/// - marked by `#[should_panic]` attribute
///
/// In the function signature, where you define your tests inputs, you can also destructuring
/// the values like any other rust function.
///
/// If the test function is an [`async` function](#async) `rstest` will run all tests as `async`
/// tests. You can use it just with `async-std` and you should include `attributes` in
/// `async-std`'s features.
///
/// In your test function you can:
///
/// - [injecting fixtures](#injecting-fixtures)
/// - Generate [parametrized test cases](#test-parametrized-cases)
/// - Generate tests for each combination of [value lists](#values-lists)
///
/// ## Injecting Fixtures
///
/// The simplest case is write a test that can be injected with
/// [`[fixture]`](macro@fixture)s. You can just declare all used fixtures by passing
/// them as a function's arguments. This can help your test to be neat
/// and make your dependency clear.
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn injected() -> i32 { 42 }
///
/// #[rstest]
/// fn the_test(injected: i32) {
///     assert_eq!(42, injected)
/// }
/// ```
///
/// [`[rstest]`](macro@rstest) procedural macro will desugar it to something that isn't
/// so far from
///
/// ```
/// #[test]
/// fn the_test() {
///     let injected=injected();
///     assert_eq!(42, injected)
/// }
/// ```
///
/// If you want to use long and descriptive names for your fixture but prefer to use
/// shorter names inside your tests you use rename feature described in
/// [fixture rename](attr.fixture.html#rename):
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn long_and_boring_descriptive_name() -> i32 { 42 }
///
/// #[rstest]
/// fn the_test(#[from(long_and_boring_descriptive_name)] short: i32) {
///     assert_eq!(42, short)
/// }
/// ```
///
/// The use of `#[from(...)]` attribute is mandatory if you need to destructure the value:
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn tuple() -> (u32, f32) { (42, 42.0) }
///
/// #[rstest]
/// fn the_test(#[from(tuple)] (u, _): (u32, f32)) {
///     assert_eq!(42, u)
/// }
/// ```
///
/// Sometimes is useful to have some parameters in your fixtures but your test would
/// override the fixture's default values in some cases. Like in
/// [fixture partial injection](attr.fixture.html#partial-injection) you use `#[with]`
/// attribute to indicate some fixture's arguments also in `rstest`.
///
/// ```
/// # struct User(String, u8);
/// # impl User { fn name(&self) -> &str {&self.0} }
/// use rstest::*;
///
/// #[fixture]
/// fn user(
///     #[default("Alice")] name: impl AsRef<str>,
///     #[default(22)] age: u8
/// ) -> User { User(name.as_ref().to_owned(), age) }
///
/// #[rstest]
/// fn check_user(#[with("Bob")] user: User) {
///     assert_eq("Bob", user.name())
/// }
/// ```
///
/// ## Test Parametrized Cases
///
/// If you would execute your test for a set of input data cases
/// you can define the arguments to use and the cases list. Let see
/// the classical Fibonacci example. In this case we would give the
/// `input` value and the `expected` result for a set of cases to test.
///
/// ```
/// use rstest::rstest;
///
/// #[rstest]
/// #[case(0, 0)]
/// #[case(1, 1)]
/// #[case(2, 1)]
/// #[case(3, 2)]
/// #[case(4, 3)]
/// fn fibonacci_test(#[case] input: u32,#[case] expected: u32) {
///     assert_eq!(expected, fibonacci(input))
/// }
///
/// fn fibonacci(input: u32) -> u32 {
///     match input {
///         0 => 0,
///         1 => 1,
///         n => fibonacci(n - 2) + fibonacci(n - 1)
///     }
/// }
/// ```
///
/// `rstest` will produce 5 independent tests and not just one that
/// check every case. Every test can fail independently and `cargo test`
/// will give follow output:
///
/// ```text
/// running 5 tests
/// test fibonacci_test::case_1 ... ok
/// test fibonacci_test::case_2 ... ok
/// test fibonacci_test::case_3 ... ok
/// test fibonacci_test::case_4 ... ok
/// test fibonacci_test::case_5 ... ok
///
/// test result: ok. 5 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
/// ```
///
/// The cases input values can be arbitrary Rust expressions that return the
/// argument type.
///
/// ```
/// use rstest::rstest;
///  
/// fn sum(a: usize, b: usize) -> usize { a + b }
///
/// #[rstest]
/// #[case("foo", 3)]
/// #[case(String::from("foo"), 2 + 1)]
/// #[case(format!("foo"), sum(2, 1))]
/// fn test_len(#[case] s: impl AsRef<str>,#[case] len: usize) {
///     assert_eq!(s.as_ref().len(), len);
/// }
/// ```
/// ### Feature flagged cases
///
/// In case you want certain test cases to only be present if a certain feature is
/// enabled, use `#[cfg_attr(feature = …, case(…))]`:
///
/// ```
/// use rstest::rstest;
///
/// #[rstest]
/// #[case(2, 2)]
/// #[cfg_attr(feature = "frac", case(4/2, 2))]
/// #[case(4/2, 2)]
/// fn it_works(#[case] a: u32, #[case] b: u32) {
///     assert!(a == b);
/// }
/// ```
///
/// This also works with [`rstest_reuse`](https://crates.io/crates/rstest_reuse).
///
/// ### Magic Conversion
///
/// You can use the magic conversion feature every time you would define a variable
/// where its type define `FromStr` trait: test will parse the string to build the value.
///
/// ```
/// # use rstest::rstest;
/// # use std::path::PathBuf;
/// # fn count_words(path: PathBuf) -> usize {0}
/// #[rstest]
/// #[case("resources/empty", 0)]
/// #[case("resources/divine_comedy", 101.698)]
/// fn test_count_words(#[case] path: PathBuf, #[case] expected: usize) {
///     assert_eq!(expected, count_words(path))
/// }
/// ```
///
/// ### Optional case description
///
/// Optionally you can give a _description_ to every case simple by follow `case`
/// with `::my_case_description` where `my_case_description` should be a a valid
/// Rust ident.
///
/// ```
/// # use rstest::*;
/// #[rstest]
/// #[case::zero_base_case(0, 0)]
/// #[case::one_base_case(1, 1)]
/// #[case(2, 1)]
/// #[case(3, 2)]
/// fn fibonacci_test(#[case] input: u32,#[case] expected: u32) {
///     assert_eq!(expected, fibonacci(input))
/// }
///
/// # fn fibonacci(input: u32) -> u32 {
/// #     match input {
/// #         0 => 0,
/// #         1 => 1,
/// #         n => fibonacci(n - 2) + fibonacci(n - 1)
/// #     }
/// # }
/// ```
///
/// Output will be
/// ```text
/// running 4 tests
/// test fibonacci_test::case_1_zero_base_case ... ok
/// test fibonacci_test::case_2_one_base_case ... ok
/// test fibonacci_test::case_3 ... ok
/// test fibonacci_test::case_4 ... ok
///
/// test result: ok. 4 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
/// ```
///
/// ### Use specific `case` attributes
///
/// Every function's attributes that preceding a `#[case]` attribute will
/// be used in this test case and all function's attributes that follow the
/// last `#[case]` attribute will mark all test cases.
///
/// This feature can be use to mark just some cases as `should_panic`
/// and choose to have a fine grain on expected panic messages.
///
/// In follow example we run 3 tests where the first pass without any
/// panic, in the second we catch a panic but we don't care about the message
/// and in the third one we also check the panic message.
///
/// ```
/// use rstest::rstest;
///
/// #[rstest]
/// #[case::no_panic(0)]
/// #[should_panic]
/// #[case::panic(1)]
/// #[should_panic(expected="expected")]
/// #[case::panic_with_message(2)]
/// fn attribute_per_case(#[case] val: i32) {
///     match val {
///         0 => assert!(true),
///         1 => panic!("No catch"),
///         2 => panic!("expected"),
///         _ => unreachable!(),
///     }
/// }
/// ```
///
/// Output:
///
/// ```text
/// running 3 tests
/// test attribute_per_case::case_1_no_panic ... ok
/// test attribute_per_case::case_3_panic_with_message ... ok
/// test attribute_per_case::case_2_panic ... ok
///
/// test result: ok. 3 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
/// ```
///
/// To mark all your tests as `#[should_panic]` use:
///
/// ```
/// # use rstest::rstest;
/// #[rstest]
/// #[case(1)]
/// #[case(2)]
/// #[case(3)]
/// #[should_panic]
/// fn fail(#[case] v: u32) { assert_eq!(0, v) }
/// ```
///
/// ## Values Lists
///
/// Another useful way to write a test and execute it for some values
/// is to use the values list syntax. This syntax can be useful both
/// for a plain list and for testing all combination of input arguments.
///
/// ```
/// # use rstest::*;
/// # fn is_valid(input: &str) -> bool { true }
///
/// #[rstest]
/// fn should_be_valid(
///     #[values("John", "alice", "My_Name", "Zigy_2001")]
///     input: &str
/// ) {
///     assert!(is_valid(input))
/// }
/// ```
///
/// or
///
/// ```
/// # use rstest::*;
/// # fn valid_user(name: &str, age: u8) -> bool { true }
///
/// #[rstest]
/// fn should_accept_all_corner_cases(
///     #[values("J", "A", "A________________________________________21")]
///     name: &str,
///     #[values(14, 100)]
///     age: u8
/// ) {
///     assert!(valid_user(name, age))
/// }
/// ```
/// where `cargo test` output is
///
/// ```text
/// test should_accept_all_corner_cases::name_1___J__::age_2_100 ... ok
/// test should_accept_all_corner_cases::name_2___A__::age_1_14 ... ok
/// test should_accept_all_corner_cases::name_2___A__::age_2_100 ... ok
/// test should_accept_all_corner_cases::name_3___A________________________________________21__::age_2_100 ... ok
/// test should_accept_all_corner_cases::name_3___A________________________________________21__::age_1_14 ... ok
/// test should_accept_all_corner_cases::name_1___J__::age_1_14 ... ok
///
/// test result: ok. 6 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
/// ```
/// Note that the test names contains the given expression sanitized into
/// a valid Rust identifier name. This should help to identify which case fails.
///
///
/// Also value list implements the magic conversion feature: every time the value type
/// implements `FromStr` trait you can use a literal string to define it.
///
/// ```
/// # use rstest::rstest;
/// # use std::net::SocketAddr;
/// #[rstest]
/// fn given_port(#[values("1.2.3.4:8000", "4.3.2.1:8000", "127.0.0.1:8000")] addr: SocketAddr) {
///     assert_eq!(8000, addr.port())
/// }
/// ```
///
/// ## Destructuring inputs
///
/// Both paramtrized case and values can be destructured:
///
/// ```
/// # use rstest::*;
/// struct S {
///     first: u32,
///     second: u32,
/// }
///
/// struct T(i32);
///
/// #[rstest]
/// #[case(S{first: 21, second: 42})]
/// fn some_test(#[case] S{first, second} : S, #[values(T(-1), T(1))] T(t): T) {
///     assert_eq!(1, t * t);
///     assert_eq!(2 * first, second);
/// }
/// ```
///
/// ## Files path as input arguments
///
/// If you need to create a test for each file in a given location you can use
/// `#[files("glob path syntax")]` attribute to generate a test for each file that
/// satisfy the given glob path.
///
/// ```
/// # use rstest::rstest;
/// # use std::path::{Path, PathBuf};
/// # fn check_file(path: &Path) -> bool { true };
/// #[rstest]
/// fn for_each_file(#[files("src/**/*.rs")] #[exclude("test")] path: PathBuf) {
///     assert!(check_file(&path))
/// }
/// ```
/// The default behavior is to ignore the files that start with `"."`, but you can
/// modify this by use `#[include_dot_files]` attribute. The `files` attribute can be
/// used more than once on the same variable, and you can also create some custom
/// exclusion rules with the `#[exclude("regex")]` attributes that filter out all
/// paths that verify the regular expression.
///
/// Sometime is useful to have test files in a workspace folder to share them between the
/// crates in your workspace. You can do that by use the usual parent folders `..` in
/// the glob path. In this case the test names will be the relative path from the crate root
/// where the parent folder components are replaced by `_UP`: for instance if you have a
/// `valid_call.yaml` in the folder `../test_cases` (from your crate root) a test name could be
/// `path_1__UP_test_cases_valid_call_yaml`.
///
/// ## Use Parametrize definition in more tests
///
/// If you need to use a test list for more than one test you can use
/// [`rstest_reuse`](https://crates.io/crates/rstest_reuse) crate.
/// With this helper crate you can define a template and use it everywhere.
///
/// ```rust,ignore
/// use rstest::rstest;
/// use rstest_reuse::{self, *};
///
/// #[template]
/// #[rstest]
/// #[case(2, 2)]
/// #[case(4/2, 2)]
/// fn two_simple_cases(#[case] a: u32, #[case] b: u32) {}
///
/// #[apply(two_simple_cases)]
/// fn it_works(#[case] a: u32,#[case] b: u32) {
///     assert_eq!(a, b);
/// }
/// ```
///
/// See [`rstest_reuse`](https://crates.io/crates/rstest_reuse) for more details.
///
/// ## Async
///
/// `rstest` provides out of the box `async` support. Just mark your
/// test function as `async` and it'll use `#[async-std::test]` to
/// annotate it. This feature can be really useful to build async
/// parametric tests using a tidy syntax:
///
/// ```
/// use rstest::*;
/// # async fn async_sum(a: u32, b: u32) -> u32 { a + b }
///
/// #[rstest]
/// #[case(5, 2, 3)]
/// #[should_panic]
/// #[case(42, 40, 1)]
/// async fn my_async_test(#[case] expected: u32, #[case] a: u32, #[case] b: u32) {
///     assert_eq!(expected, async_sum(a, b).await);
/// }
/// ```
///
/// Currently only `async-std` is supported out of the box. But if you need to use
/// another runtime that provide it's own test attribute (i.e. `tokio::test` or
/// `actix_rt::test`) you can use it in your `async` test like described in
/// [Inject Test Attribute](attr.rstest.html#inject-test-attribute).
///
/// To use this feature, you need to enable `attributes` in the `async-std`
/// features list in your `Cargo.toml`:
///
/// ```toml
/// async-std = { version = "1.5", features = ["attributes"] }
/// ```
///
/// If your test input is an async value (fixture or test parameter) you can use `#[future]`
/// attribute to remove `impl Future<Output = T>` boilerplate and just use `T`:
///
/// ```
/// use rstest::*;
/// #[fixture]
/// async fn base() -> u32 { 42 }
///
/// #[rstest]
/// #[case(21, async { 2 })]
/// #[case(6, async { 7 })]
/// async fn my_async_test(#[future] base: u32, #[case] expected: u32, #[future] #[case] div: u32) {
///     assert_eq!(expected, base.await / div.await);
/// }
/// ```
///
/// As you noted you should `.await` all _future_ values and this some times can be really boring.
/// In this case you can use `#[future(awt)]` to _awaiting_ an input or annotating your function
/// with `#[awt]` attributes to globally `.await` all your _future_ inputs. Previous code can be
/// simplified like follow:
///
/// ```
/// use rstest::*;
/// # #[fixture]
/// # async fn base() -> u32 { 42 }
///
/// #[rstest]
/// #[case(21, async { 2 })]
/// #[case(6, async { 7 })]
/// #[awt]
/// async fn global(#[future] base: u32, #[case] expected: u32, #[future] #[case] div: u32) {
///     assert_eq!(expected, base / div);
/// }
///
/// #[rstest]
/// #[case(21, async { 2 })]
/// #[case(6, async { 7 })]
/// async fn single(#[future] base: u32, #[case] expected: u32, #[future(awt)] #[case] div: u32) {
///     assert_eq!(expected, base.await / div);
/// }
/// ```
///
/// ### Default timeout
///
/// You can set a default timeout for test using the `RSTEST_TIMEOUT` environment variable.
/// The value is in seconds and is evaluated on test compile time.///
///
/// ### Test `#[timeout()]`
///
/// You can define an execution timeout for your tests with `#[timeout(<duration>)]` attribute. Timeout
/// works both for sync and async tests and is runtime agnostic. `#[timeout(<duration>)]` take an
/// expression that should return a `std::time::Duration`. Follow a simple async example:
///
/// ```rust
/// use rstest::*;
/// use std::time::Duration;
///
/// async fn delayed_sum(a: u32, b: u32,delay: Duration) -> u32 {
///     async_std::task::sleep(delay).await;
///     a + b
/// }
///
/// #[rstest]
/// #[timeout(Duration::from_millis(80))]
/// async fn single_pass() {
///     assert_eq!(4, delayed_sum(2, 2, ms(10)).await);
/// }
/// ```
/// In this case test pass because the delay is just 10 milliseconds and timeout is
/// 80 milliseconds.
///
/// You can use `timeout` attribute like any other attribute in your tests, and you can
/// override a group timeout with a test specific one. In the follow example we have
/// 3 tests where first and third use 100 milliseconds but the second one use 10 milliseconds.
/// Another valuable point in this example is to use an expression to compute the
/// duration.
///
/// ```rust
/// # use rstest::*;
/// # use std::time::Duration;
/// #
/// # async fn delayed_sum(a: u32, b: u32,delay: Duration) -> u32 {
/// #     async_std::task::sleep(delay).await;
/// #     a + b
/// # }
/// fn ms(ms: u32) -> Duration {
///     Duration::from_millis(ms.into())
/// }
///
/// #[rstest]
/// #[case::pass(ms(1), 4)]
/// #[timeout(ms(10))]
/// #[case::fail_timeout(ms(60), 4)]
/// #[case::fail_value(ms(1), 5)]
/// #[timeout(ms(100))]
/// async fn group_one_timeout_override(#[case] delay: Duration, #[case] expected: u32) {
///     assert_eq!(expected, delayed_sum(2, 2, delay).await);
/// }
/// ```
///
/// If you want to use `timeout` for `async` test you need to use `async-timeout`
/// feature (enabled by default).
///
/// ## Inject Test Attribute
///
/// If you would like to use another `test` attribute for your test you can simply
/// indicate it in your test function's attributes. For instance if you want
/// to test some async function with use `actix_rt::test` attribute you can just write:
///
/// ```
/// use rstest::*;
/// use actix_rt;
/// use std::future::Future;
///
/// #[rstest]
/// #[case(2, async { 4 })]
/// #[case(21, async { 42 })]
/// #[actix_rt::test]
/// async fn my_async_test(#[case] a: u32, #[case] #[future] result: u32) {
///     assert_eq!(2 * a, result.await);
/// }
/// ```
/// Just the attributes that ends with `test` (last path segment) can be injected:
/// in this case the `#[actix_rt::test]` attribute will replace the standard `#[test]`
/// attribute.
///
/// Some test attributes allow to inject arguments into the test function, in a similar way to rstest.
/// This can lead to compile errors when rstest is not able to resolve the additional arguments.
/// To avoid this, see [Ignoring Arguments](attr.rstest.html#ignoring-arguments).
///
/// ## Local lifetime and `#[by_ref]` attribute
///
/// In some cases you may want to use a local lifetime for some arguments of your test.
/// In these cases you can use the `#[by_ref]` attribute then use the reference instead
/// the value.
///
/// ```rust
/// # use std::cell::Cell;
/// # use rstest::*;
/// # #[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// enum E<'a> {
///     A(bool),
///     B(&'a Cell<E<'a>>),
/// }
///
/// fn make_e_from_bool<'a>(_bump: &'a (), b: bool) -> E<'a> {
///     E::A(b)
/// }
///
/// #[fixture]
/// fn bump() -> () {}
///  
/// #[rstest]
/// #[case(true, E::A(true))]
/// fn it_works<'a>(#[by_ref] bump: &'a (), #[case] b: bool, #[case] expected: E<'a>) {
///     let actual = make_e_from_bool(&bump, b);
///     assert_eq!(actual, expected);
/// }
/// ```
///
/// You can use `#[by_ref]` attribute for all arguments of your test and not just for fixture
/// but also for cases, values and files.
///
/// ## Putting all Together
///
/// All these features can be used together with a mixture of fixture variables,
/// fixed cases and bunch of values. For instance, you might need two
/// test cases which test for panics, one for a logged in user and one for a guest user.
///
/// ```rust
/// # enum User { Guest, Logged, }
/// # impl User { fn logged(_n: &str, _d: &str, _w: &str, _s: &str) -> Self { Self::Logged } }
/// # struct Item {}
/// # trait Repository { fn find_items(&self, user: &User, query: &str) -> Result<Vec<Item>, String> { Err("Invalid query error".to_owned()) } }
/// # #[derive(Default)] struct InMemoryRepository {}
/// # impl Repository for InMemoryRepository {}
///
/// use rstest::*;
///
/// #[fixture]
/// fn repository() -> InMemoryRepository {
///     let mut r = InMemoryRepository::default();
///     // fill repository with some data
///     r
/// }
///
/// #[fixture]
/// fn alice() -> User {
///     User::logged("Alice", "2001-10-04", "London", "UK")
/// }
///
/// #[rstest]
/// #[case::authorized_user(alice())] // We can use `fixture` also as standard function
/// #[case::guest(User::Guest)]   // We can give a name to every case : `guest` in this case
/// #[should_panic(expected = "Invalid query error")] // We would test a panic
/// fn should_be_invalid_query_error(
///     repository: impl Repository,
///     #[case] user: User,
///     #[values("     ", "^%$some#@invalid!chars", ".n.o.d.o.t.s.")] query: &str,
///     query: &str
/// ) {
///     repository.find_items(&user, query).unwrap();
/// }
/// ```
///
/// ## Ignoring Arguments
///
/// Sometimes, you may want to inject and use fixtures not managed by rstest
/// (e.g. db connection pools for sqlx tests).
///
/// In these cases, you can use the `#[ignore]` attribute to ignore the additional
/// parameter and let another crate take care of it:
///
/// ```rust, ignore
/// use rstest::*;
/// use sqlx::*;
///
/// #[fixture]
/// fn my_fixture() -> i32 { 42 }
///
/// #[rstest]
/// #[sqlx::test]
/// async fn test_db(my_fixture: i32, #[ignore] pool: PgPool) {
///     assert_eq!(42, injected);
///     // do stuff with the connection pool
/// }
/// ```
///
///
/// ## Trace Input Arguments
///
/// Sometimes can be very helpful to print all test's input arguments. To
/// do it you can use the `#[trace]` function attribute that you can apply
/// to all cases or just to some of them.
///
/// ```
/// use rstest::*;
///
/// #[fixture]
/// fn injected() -> i32 { 42 }
///
/// #[rstest]
/// #[trace]
/// fn the_test(injected: i32) {
///     assert_eq!(42, injected)
/// }
/// ```
///
/// Will print an output like
///
/// ```bash
/// Testing started at 14.12 ...
/// ------------ TEST ARGUMENTS ------------
/// injected = 42
/// -------------- TEST START --------------
///
///
/// Expected :42
/// Actual   :43
/// ```
/// But
/// ```
/// # use rstest::*;
/// #[rstest]
/// #[case(1)]
/// #[trace]
/// #[case(2)]
/// fn the_test(#[case] v: i32) {
///     assert_eq!(0, v)
/// }
/// ```
/// will trace just `case_2` input arguments.
///
/// If you want to trace input arguments but skip some of them that don't
/// implement the `Debug` trait, you can also use the
/// `#[notrace]` argument attribute to skip them:
///
/// ```
/// # use rstest::*;
/// # struct Xyz;
/// # struct NoSense;
/// #[rstest]
/// #[trace]
/// fn the_test(injected: i32, #[notrace] xyz: Xyz, #[notrace] have_no_sense: NoSense) {
///     assert_eq!(42, injected)
/// }
/// ```
/// # Old _compact_ syntax
///
/// `rstest` support also a syntax where all options and configuration can be write as
/// `rstest` attribute arguments. This syntax is a little less verbose but make
/// composition harder: for instance try to add some cases to a `rstest_reuse` template
/// is really hard.
///
/// So we'll continue to maintain the old syntax for a long time but we strongly encourage
/// to switch your test in the new form.
///
/// Anyway, here we recall this syntax and rewrite the previous example in the _compact_ form.
///
/// ```text
/// rstest(
///     arg_1,
///     ...,
///     arg_n[,]
///     [::attribute_1[:: ... [::attribute_k]]]
/// )
/// ```
/// Where:
///
/// - `arg_i` could be one of the follow
///   - `ident` that match to one of function arguments for parametrized cases
///   - `case[::description](v1, ..., vl)` a test case
///   - `fixture(v1, ..., vl) [as argument_name]` where fixture is the injected
///     fixture and argument_name (default use fixture) is one of function arguments
///     that and `v1, ..., vl` is a partial list of fixture's arguments
///   - `ident => [v1, ..., vl]` where `ident` is one of function arguments and
///     `v1, ..., vl` is a list of values for ident
/// - `attribute_j` a test attribute like `trace` or `notrace`
///
/// ## Fixture Arguments
///
/// ```
/// # struct User(String, u8);
/// # impl User { fn name(&self) -> &str {&self.0} }
/// # use rstest::*;
/// #
/// # #[fixture]
/// # fn user(
/// #     #[default("Alice")] name: impl AsRef<str>,
/// #     #[default(22)] age: u8
/// # ) -> User { User(name.as_ref().to_owned(), age) }
/// #
/// #[rstest(user("Bob"))]
/// fn check_user(user: User) {
///     assert_eq("Bob", user.name())
/// }
/// ```
///
/// ## Fixture Rename
/// ```
/// # use rstest::*;
/// #[fixture]
/// fn long_and_boring_descriptive_name() -> i32 { 42 }
///
/// #[rstest(long_and_boring_descriptive_name as short)]
/// fn the_test(short: i32) {
///     assert_eq!(42, short)
/// }
/// ```
///
/// ## Parametrized
///
/// ```
/// # use rstest::*;
/// #[rstest(input, expected,
///     case::zero_base_case(0, 0),
///     case::one_base_case(1, 1),
///     case(2, 1),
///     case(3, 2),
///     #[should_panic]
///     case(4, 42)
/// )]
/// fn fibonacci_test(input: u32, expected: u32) {
///     assert_eq!(expected, fibonacci(input))
/// }
///
/// # fn fibonacci(input: u32) -> u32 {
/// #     match input {
/// #         0 => 0,
/// #         1 => 1,
/// #         n => fibonacci(n - 2) + fibonacci(n - 1)
/// #     }
/// # }
/// ```
///
/// ## Values Lists
///
/// ```
/// # use rstest::*;
/// # fn is_valid(input: &str) -> bool { true }
///
/// #[rstest(
///     input => ["John", "alice", "My_Name", "Zigy_2001"]
/// )]
/// fn should_be_valid(input: &str) {
///     assert!(is_valid(input))
/// }
/// ```
///
/// ## `trace` and `notrace`
///
/// ```
/// # use rstest::*;
/// # struct Xyz;
/// # struct NoSense;
/// #[rstest(::trace::notrace(xzy, have_no_sense))]
/// fn the_test(injected: i32, xyz: Xyz, have_no_sense: NoSense) {
///     assert_eq!(42, injected)
/// }
/// ```
///
pub use rstest_macros::rstest;
