# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

## 0.22.0 - 2025-09-08

### Added
- New optional alternate `transform` syntax using a full fn, to allow support for custom lifetimes, generics and a where clause to custom builder method.

Example:
```rust
#[derive(TypedBuilder)]
struct Foo {
    #[builder(
        setter(
            fn transform<'a, M>(value: impl IntoValue<'a, String, M>) -> String
            where
              M: std::fmt::Display
            {
                value.into_value()
            },
        )
    )]
    s: String,
}
```

## 0.21.2 - 2025-08-21
### Fixed
- Recognize `TypeGroup` when checking for `Option`.

## 0.21.1 - 2025-08-12
### Fixed
- Strip raw ident prefix from base method name before assembling prefixed/suffixed fallback method names

## 0.21.0 - 2025-03-20
### Added
- Added `ignore_invalid` option to `strip_option` to skip stripping for non-Option fields
- Added `fallback_prefix` and `fallback_suffix` options to `strip_option` for customizing fallback method names
- Added support for field defaults with `strip_option` and its fallback options

### Changed
- Improved handling of `strip_option` to work better with field defaults
- Made `strip_option` more flexible with non-Option fields when `ignore_invalid` is set

## 0.20.1 - 2025-03-14
### Fixed
- Fix mutator with type parameter using associated type (see issue #157)

## 0.20.0 - 2024-08-22
### Added
- Add `#[builder(setter(strip_option(fallback = field_opt)))]` to add a fallback unstripped method to the builder struct.
- Add `#[builder(setter(strip_bool(fallback = field_bool)))]` to add a fallback setter that takes the `bool` value to the builder struct.

## 0.19.1 - 2024-07-14
### Fixed
- Fix mutators for generic fields (see issue #149)

## 0.19.0 - 2024-06-15
### Added
- Use fields' doc comments for the setters.

## 0.18.2 - 2024-04-16
### Fixed
- Also add the licenses to the macro crate.

## 0.18.1 - 2024-01-17
### Fixed
- Add `#[allow(clippy::no_effect_underscore_binding)]` to generated methods
  that need to destructure intermediate builder state.
- Use a proper `OR` syntax for the dual license.

## 0.18.0 - 2023-10-19
### Fixed
- `?Sized` generic parameters are now supported.

## 0.17.0 - 2023-10-15
### Changed
- Internal refactor of attribute parsing - results in better error messages and
  easier proces for adding new settings.

### Added
- `#[builder(mutators(...))]` to generate functions on builder to mutate fields
- `#[builder(via_mutator)]` on fields to allow defining fields initialized
  during `::builder()` for use with `mutators`
- `mutable_during_default_resolution` to allow `default` expression mutate
  previous fields.

### Fixed
- Add support for paths with angle brackets (see PR #122 )

## 0.16.2 - 2023-09-22
### Fixed
- Use generics with the constructor in `build` method (see issue #118)

## 0.16.1 - 2023-09-18
### Fixed
- Add `#[allow(clippy::exhaustive_enums)]` to generated empty enums used for
  error "reporting" (see issue #112)
- Add `#[automatically_derived]` to generated `impl`s (see issue #114)
- Add `#[allow(clippy::used_underscore_binding)]` to build method and setter
  methods (see issue #113)

## 0.16.0 - 2023-08-26
### Added
- `#[builder(crate_module_path = ...)]` for overcoming cases where the derive
  macro is used in another crate's macro (see issue #109)

## 0.15.2 - 2023-08-03
### Fixed
- Fix const generics generating "empty" entries in some lists, resulting in
  consecutive commas (see issue #106)

## 0.15.1 - 2023-07-10
### Fixed
- no-std build.

## 0.15.0 - 2023-07-06
### Changed
- [**BREAKING**] Split the derive macro out to [a separate procmacro
  crate](https://crates.io/crates/typed-builder-macro). This is considered a
  breaking change because reexporting and/or renmaing the crate can now prevent
  the generated code from finding the types it needs (see issue #101)

### Fixed
- Marking a field as `#[deprecated]` now behaves properly - `TypedBuilder`
  generated code itself does trigger the deprecation warning, and instead the
  setter for that field now does.
- The "fake" `build` method when required fields are not provided now returns
  the never type ("`!`"). Refer to PR #97 for more thorough explanation.

### Added
- Support for setter method prefixes and suffixes `#[builder(field_defaults(setter(prefix = "...", suffix = "...")))]`.
  This either prepends or appends the provided string to the setter method. This allows method names like: `set_x()`,
  `with_y()`, or `set_z_value()`.

## 0.14.0 - 2023-03-08
### Added
- `build_method(into)` and `build_method(into = ...)`.

## 0.13.0 - 2023-03-05
### Changed
y
- [**BREAKING**] Builder state parameter moved to the end of the generated builder type's parameters list.
- Generated builder type's builder state parameter now defaults to tuple of
  empty tuples. This means the empty builder, where no parameter is yet set.

### Fixed
- `#[builder(build_method(...))]` now affects the fake `build` method that's
  generated to add information to the compiler error.

## 0.12.0 - 2023-01-29
### Removed
- [**BREAKING**] `builder_method_doc = "..."`, `builder_type_doc = "..."` and
  `build_method_doc = "..."` are replaced with `builder_method(doc = "...")`,
  `builder_type(doc = "...")` and `build_method(doc = "...")`.

### Added
- `build_method(...)` now has a `doc` field.
- `builder_method(...)` and `builder_type(...)`, which are structured similarly to `build_method(...)`.

## 0.11.0 - 2022-10-29
### Added
- `#[builder(build_method(vis="pub", name=build))]` for customizing visibility and fn name of the final build method
  (the default visibility is `pub`, and default build name is `build`)

## 0.10.0 - 2022-02-13
### Added
- `#[builder(setter(strip_bool))]` for making zero arguments setters for `bool` fields that just
  set them to `true` (the `default` automatically becomes `false`)

## 0.9.1 - 2021-09-04
### Fixed
- Add `extern crate proc_macro;` to solve some weird problem (https://github.com/idanarye/rust-typed-builder/issues/57)
- Use unambiguous `::` prefixed absolute paths in generated code.

## 0.9.0 - 2021-01-31
### Added
- Builder type implements `Clone` when all set fields support clone.
- `#[builder(setter(transform = ...))]` attribute for running a transform on a
  setter's argument to convert them to the field's type.

### Fixed
- Fix code generation for raw identifiers.

## 0.8.0 - 2020-12-06
### Changed
- Upgraded the Rust edition to 2018.

### Added
- `#[field_defaults(...)]` attribute for settings default attributes for all
  the fields.

## 0.7.1 - 2020-11-20
### Fixed
- Fix lifetime bounds erroneously preserved in phantom generics.

## 0.7.0 - 2020-07-23
### Added
- Brought back `default_code`, because it needed to resolve conflict with other
  custom derive proc-macro crates that try to parse `[#builder(default = ...)]`
  attribute in order to decide if they are relevant to them - and fail because
  the expect them to be simple literals.

## 0.6.0 - 2020-05-18
### Added
- Ability to use `into` and `strip_option` simultaneously for a field.

### Changed
- [**BREAKING**] Specifying `skip` twice in the same `builder(setter(...))` is
  no longer supported. Then again, if you were doing that you probably deserve
  having your code broken.

## 0.5.1 - 2020-01-26
### Fixed
- Prevent Clippy from warning about the `panic!()` in the faux build method.

## 0.5.0 - 2020-01-25
### Changed
- [**BREAKING**] Move `doc` and `skip` into a subsetting named `setter(...)`.
  This means that `#[builder(doc = "...")]`, for example, should now be written
  as `#[builder(setter(doc = "..."))]`.
- [**BREAKING**] Setter arguments by default are no longer automatically
  converted to the target type with `into()`. If you want to automatically
  convert them, use `#[builder(setter(into))]`. This new default enables rustc
  inference for generic types and proper integer literal type detection.
- Improve build errors for incomplete `.build()` and repeated setters, by
  creating faux methods with deprecation warnings.

### Added
- `#[builder(setter(strip_option))]` for making setters for `Option` fields
  automatically wrap the argument with `Some(...)`. Note that this is a weaker
  conversion than `#[builder(setter(into))]`, and thus can still support type
  inference and integer literal type detection.

### Removed
- [**BREAKING**] Removed the `default_code` setting (`#[builder(default_code =
  "...")]`) because it is no longer required now that Rust and `syn` support
  arbitrary expressions in attributes.

## 0.4.1 - 2020-01-17
### Fixed
- [**BREAKING**] now state types are placed before original generic types.
  Previously, all state types are appended to generic arguments. For example,
  `Foo<'a, X, Y>` yields `FooBuilder<'a, X, Y, ((), ())>` **previously**, and
  now it becomes `FooBuilder<'a, ((), ()), X, Y, >.`. This change fix compiler error
  for struct with default type like `Foo<'a, X, Y=Bar>`. Rust only allow type
  parameters with a default to be trailing.

## 0.4.0 - 2019-12-13
### Added
- `#![no_std]` is now supported out of the box. (You don't need to opt into any
  features, it just works.)
- [**BREAKING**] a `default_code` expression can now refer to the values of
  earlier fields by name (This is extremely unlikely to break your code, but
  could in theory due to shadowing)
- `#[builder(skip)]` on fields, to not provide a method to set that field.
- Control of documentation:
  - `#[builder(doc = "…")]` on fields, to document the field's method on the
    builder. Unlike `#[doc]`, you can currently only have one value rather than
    one attribute per line; but that's not a big deal since you don't get to
    use the `///` sugar anyway. Just use a multiline string.
  - `#[builder(doc, builder_method_doc = "…", builder_type_doc = "…",
    build_method_doc = "…")]` on structs:
    - `doc` unhides the builder type from the documentation.
	- `builder_method_doc = "…"` replaces the default documentation that
	  will be generated for the builder() method of the type for which the
	  builder is being generated.
	- `builder_type_doc = "…"` replaces the default documentation that will
	  be generated for the builder type. Implies `doc`.
	- `build_method_doc = "…"` replaces the default documentation that will
	  be generated for the build() method of the builder type. Implies
	  `doc`.

### Changed
- [**BREAKING**] Renamed the generated builder type from
  `TypedBuilder_BuilderFor_Foo` to `FooBuilder`, for improved ergonomics,
  especially when you enable documentation of the builder type.
  - Generic identifiers were also changed, from `TypedBuilder_genericType_x` to
    `__x`. This is still expected to avoid all name collisions, but is easier
    to read in the builder type docs if you enable them.
  - Renamed the conversion helper trait for documentation purposes
    (`TypedBuilder_conversionHelperTrait_Foo` to `FooBuilder_Optional`), and
    its method name for simpler code.
- [**BREAKING**] `default_code` is now lazily evaluated instead of eagerly; any
  side-effects that there might have been will no longer occur. As is usual in
  this release, this is very unlikely to affect you.
- The restriction that there be only one `#[builder]` attribute per field has
  been lifted. You can now write `#[builder(skip)] #[builder(default)]` instead
  of `#[builder(skip, default)]` if you want to. As was already the case,
  latest definition wins.
- [**BREAKING**] Use a single generic parameter to represent the builder type's
  state (see issue #21). Previously we would use a parameter for each field.

### Changed
- Move to dual license - MIT/Apache-2.0. Previously this project was just MIT.

## 0.3.0 - 2019-02-19
### Added
- `#[builder(default_code = "...")]` syntax for defaults that cannot be parsed
  as attributes no matter what.

### Changed
- Move the docs from the crate to the custom derive proc macro.

## 0.2.0 - 2019-02-06
### Changed
- Upgraded `syn` version to support Rust 2018.
- [**BREAKING**] Changed attribute style to `#[builder(...)]`:
  - `#[default]` -> `#[builder(default)]`
  - `#[default=...]` -> `#[builder(default=...)]`
- [**BREAKING**] `default` no longer needs to be a string.
  - But you need to change your code anyways because the attribute style was changed.

## 0.1.1 - 2018-07-24
### Fixed
- Allow missing docs in structs that derive `TypedBuilder`.

## 0.1.0 - 2017-10-05
### Added
- Custom derive for generating the builder pattern.
- All setters are accepting `Into` values.
- Compile time verification that all fields are set before calling `.build()`.
- Compile time verification that no field is set more than once.
- Ability to annotate fields with `#[default]` to make them optional and specify a default value when the user does not set them.
- Generates simple documentation for the `.builder()` method.
