#![no_std]

/// `TypedBuilder` is not a real type - deriving it will generate a `::builder()` method on your
/// struct that will return a compile-time checked builder. Set the fields using setters with the
/// same name as the struct's fields and call `.build()` when you are done to create your object.
///
/// Trying to set the same fields twice will generate a compile-time error. Trying to build without
/// setting one of the fields will also generate a compile-time error - unless that field is marked
/// as `#[builder(default)]`, in which case the `::default()` value of it's type will be picked. If
/// you want to set a different default, use `#[builder(default=...)]`.
///
/// # Examples
///
/// ```
/// use typed_builder::TypedBuilder;
///
/// #[derive(PartialEq, TypedBuilder)]
/// struct Foo {
///     // Mandatory Field:
///     x: i32,
///
///     // #[builder(default)] without parameter - use the type's default
///     // #[builder(setter(strip_option))] - wrap the setter argument with `Some(...)`
///     #[builder(default, setter(strip_option))]
///     y: Option<i32>,
///
///     // Or you can set the default
///     #[builder(default=20)]
///     z: i32,
/// }
///
/// assert!(
///     Foo::builder().x(1).y(2).z(3).build()
///     == Foo { x: 1, y: Some(2), z: 3, });
///
/// // Change the order of construction:
/// assert!(
///     Foo::builder().z(1).x(2).y(3).build()
///     == Foo { x: 2, y: Some(3), z: 1, });
///
/// // Optional fields are optional:
/// assert!(
///     Foo::builder().x(1).build()
///     == Foo { x: 1, y: None, z: 20, });
///
/// // This will not compile - because we did not set x:
/// // Foo::builder().build();
///
/// // This will not compile - because we set y twice:
/// // Foo::builder().x(1).y(2).y(3);
/// ```
///
/// # Customization with attributes
///
/// In addition to putting `#[derive(TypedBuilder)]` on a type, you can specify a `#[builder(...)]`
/// attribute on the type, and on any fields in it.
///
/// On the **type**, the following values are permitted:
///
/// - `doc`: enable documentation of the builder type. By default, the builder type is given
///   `#[doc(hidden)]`, so that the `builder()` method will show `FooBuilder` as its return type,
///   but it won't be a link. If you turn this on, the builder type and its `build` method will get
///   sane defaults. The field methods on the builder will be undocumented by default.
///
/// - `crate_module_path`: This is only needed when `typed_builder` is reexported from another
///   crate - which usually happens when another macro uses it. In that case, it is the
///   reponsibility of that macro to set the `crate_module_path` to the _unquoted_ module path from
///   which the `typed_builder` crate can be accessed, so that the `TypedBuilder` macro will be
///   able to access the typed declared in it.
///
///   Defaults to `#[builder(crate_module_path=::typed_builder)]`.
///
/// - The following subsections:
///   - `builder_method(...)`: customize the builder method that creates the builder type
///   - `builder_type(...)`: customize the builder type
///   - `build_method(...)`: customize the final build method
///
///   All have the same fields:
///   - `vis = "..."`: sets the visibility of the build method, default is `pub`
///   - `name = ...`: sets the fn name of the build method, default is `build`
///   - `doc = "..."` replaces the default documentation that will be generated for the
///     `build()` method of the builder type. Setting this implies `doc`.
///
///
/// - The `build_method(...)` subsection also has:
///   - `into` or `into = ...`: change the output type of the builder. When a specific value/type
///     is set via the assignment, this will be the output type of the builder. If no specific
///     type is set, but `into` is specified, the return type will be generic and the user can
///     decide which type shall be constructed. In both cases an [`Into`] conversion is required to
///     be defined from the original type to the target type.
///
/// - The `builder_type(...)` subsection also has:
///   - `attributes` - for adding attributes to the builder type. Note that the full attribute
///     syntax is required **inside** this section -
///     ```ignore
///     #[builder(builder_type(attributes(
///         #[derive(...)]
///         #[some_other_attribute]
///     )))]
///     ```
///
///     Example:
///     
///     ```
///     use typed_builder::TypedBuilder;
///
///     #[derive(TypedBuilder)]
///     #[builder(builder_type(attributes(#[derive(Debug)])))]
///     struct Foo {
///         x: i32,
///     }
///     ```
///
/// - `field_defaults(...)` is structured like the `#[builder(...)]` attribute you can put on the
///   fields and sets default options for fields of the type. If specific field need to revert some
///   options to the default defaults they can prepend `!` to the option they need to revert, and
///   it would ignore the field defaults for that option in that field.
///
///    ```
///    use typed_builder::TypedBuilder;
///
///    #[derive(TypedBuilder)]
///    #[builder(field_defaults(default, setter(strip_option)))]
///    struct Foo {
///        // Defaults to None, options-stripping is performed:
///        x: Option<i32>,
///
///        // Defaults to 0, option-stripping is not performed:
///        #[builder(setter(!strip_option))]
///        y: i32,
///
///        // Defaults to Some(13), option-stripping is performed:
///        #[builder(default = Some(13))]
///        z: Option<i32>,
///
///        // Accepts params `(x: f32, y: f32)`
///        #[builder(setter(!strip_option, transform = |x: f32, y: f32| Point { x, y }))]
///        w: Point,
///    }
///
///    #[derive(Default)]
///    struct Point { x: f32, y: f32 }
///    ```
///
/// - `mutators(...)` takes functions, that can mutate fields inside of the builder.
///   See [mutators](#mutators) for details.
///
/// On each **field**, the following values are permitted:
///
/// - `default`: make the field optional, defaulting to `Default::default()`. This requires that
///   the field type implement `Default`. Mutually exclusive with any other form of default.
///
/// - `default = ...`: make the field optional, defaulting to the expression `...`.
///
/// - `default_where(...)`: add trait bounds to the default. This means that the `default`
///   expression (or type default) is allowed to rely on these bounds, but the field will not have
///   a default if these trait bounds are not fulfilled.
///
///   Example:
///
///    ```
///    use typed_builder::TypedBuilder;
///
///    #[derive(TypedBuilder)]
///    struct Foo<T> {
///        #[builder(default, default_where(T: Default))]
///        bar: T,
///    }
///    ```
///
/// - `default_code = "..."`: make the field optional, defaulting to the expression `...`. Note that
///   you need to enclose it in quotes, which allows you to use it together with other custom
///   derive proc-macro crates that complain about "expected literal". Note that if `...` contains
///   a string, you can use raw string literals to avoid escaping the double quotes - e.g.
///   `#[builder(default_code = r#""default text".to_owned()"#)]`.
///
/// - `via_mutators`: initialize the field when constructing the builder, useful in combination
///   with [mutators](#mutators).
///
/// - `via_mutators = ...` or `via_mutators(init = ...)`: initialies the field with the expression `...`
///   when constructing the builder, useful in combination with [mutators](#mutators).
///
/// - `mutators(...)` takes functions, that can mutate fields inside of the builder.
///   Mutators specified on a field, mark this field as required, see [mutators](#mutators) for details.
///
/// - `setter(...)`: settings for the field setters. The following values are permitted inside:
///
///   - `doc = "..."`: sets the documentation for the field's setter on the builder type. This will be
///     of no value unless you enable docs for the builder type with `#[builder(doc)]` or similar on
///     the type.
///
///   - `skip`: do not define a method on the builder for this field. This requires that a default
///     be set.
///
///   - `into`: automatically convert the argument of the setter method to the type of the field.
///     Note that this conversion interferes with Rust's type inference and integer literal
///     detection, so this may reduce ergonomics if the field type is generic or an unsigned integer.
///
///   - `strip_option`: for `Option<...>` fields only, this makes the setter wrap its argument with
///     `Some(...)`, relieving the caller from having to do this. Note that with this setting on
///     one cannot set the field to `None` with the setter - so the only way to get it to be `None`
///     is by using `#[builder(default)]` and not calling the field's setter.
///
///   - `strip_option(fallback = field_opt)`: for `Option<...>` fields only. As above this
///     still wraps the argument with `Some(...)`. The name given to the fallback method adds
///     another method to the builder without wrapping the argument in `Some`. You can now call
///     `field_opt(Some(...))` instead of `field(...)`.
///
///     The `setter(strip_option)` attribute supports several `field_defaults` features:
///
///     - `ignore_invalid`: Skip stripping for non-Option fields instead of causing a compile error
///     - `fallback_prefix`: Add a prefix to every fallback method name
///     - `fallback_suffix`: Add a suffix to every fallback method name
///
///     Example:
///
///     ```
///     use typed_builder::TypedBuilder;
///
///     #[derive(TypedBuilder)]
///     #[builder(field_defaults(setter(strip_option(
///         ignore_invalid,
///         fallback_prefix = "opt_",
///         fallback_suffix = "_val"
///     ))))]
///     struct Foo {
///         x: Option<i32>,  // Can use .x(42) or .opt_x_val(None)
///         y: i32,          // Uses .y(42) only since it's not an Option
///     }
///     ```
///
///   - `strip_bool`: for `bool` fields only, this makes the setter receive no arguments and simply
///     set the field's value to `true`. When used, the `default` is automatically set to `false`.
///
///   - `strip_bool(fallback = field_bool)`: for `bool` fields only. As above this allows passing
///     the boolean value. The name given to the fallback method adds another method to the builder
///     without where the bool value can be specified.
///
///   - `transform = |param1: Type1, param2: Type2 ...| expr`: this makes the setter accept
///     `param1: Type1, param2: Type2 ...` instead of the field type itself. The parameters are
///     transformed into the field type using the expression `expr`. The transformation is performed
///     when the setter is called. `transform` can also be provided in full `fn` syntax,
///     to allow custom lifetimes, a generic and a where clause.
///     Example:
///     ```rust
///     #[builder(
///         setter(
///             fn transform<'a, M>(value: impl IntoValue<'a, String, M>) -> String
///             where
///                 M: 'a,
///             {
///                 value.into_value()
///             },
///         )
///     )]
///     ```
///
///   - `prefix = "..."` prepends the setter method with the specified prefix. For example, setting
///     `prefix = "with_"` results in setters like `with_x` or `with_y`. This option is combinable
///     with `suffix = "..."`.
///
///   - `suffix = "..."` appends the setter method with the specified suffix. For example, setting
///     `suffix = "_value"` results in setters like `x_value` or `y_value`. This option is combinable
///     with `prefix = "..."`.
///
///   - `mutable_during_default_resolution`: when expressions in `default = ...` field attributes
///     are evaluated, this field will be mutable, allowing earlier-defined fields to be mutated by
///     later-defined fields.
///     **Warning** - Use this feature with care! If the field that mutates the previous field in
///     its `default` expression is set via a setter, that mutation will not happen.
///
/// # Mutators
/// Set fields can be mutated using mutators, these can be defind via `mutators(...)`.
///
/// Fields annotated with `#[builder(via_mutators)]` are always available to mutators. Additional fields,
/// that the mutator accesses need to be delcared using `#[mutator(requires = [field1, field2, ...])]`.
/// The mutator will only be availible to call when they are set.
///
/// Mutators on a field, result in them automatically making the field required, i.e., it needs to be
/// marked as `via_mutators`, or its setter be called. Appart from that, they behave identically.
///
/// ```
/// use typed_builder::TypedBuilder;
///
/// #[derive(PartialEq, Debug, TypedBuilder)]
/// #[builder(mutators(
///     // Mutator has only acces to fields marked as `via_mutators`.
///     fn inc_a(&mut self, a: i32){
///         self.a += a;
///     }
///     // Mutator has access to `x` additionally.
///     #[mutator(requires = [x])]
///     fn x_into_b(&mut self) {
///         self.b.push(self.x)
///     }
/// ))]
/// struct Struct {
///     // Does not require explicit `requires = [x]`, as the field
///     // the mutator is specifed on, is required implicitly.
///     #[builder(mutators(
///         fn x_into_b_field(self) {
///             self.b.push(self.x)
///         }
///     ))]
///     x: i32,
///     #[builder(via_mutators(init = 1))]
///     a: i32,
///     #[builder(via_mutators)]
///     b: Vec<i32>
/// }
///
/// // Mutators do not enforce only being called once
/// assert_eq!(
///     Struct::builder().x(2).x_into_b().x_into_b().x_into_b_field().inc_a(2).build(),
///     Struct {x: 2, a: 3, b: vec![2, 2, 2]});
/// ```
pub use typed_builder_macro::TypedBuilder;

#[doc(hidden)]
pub trait NextFieldDefault<TypedBuilderExistingFields> {
    type Output;

    fn resolve(input: TypedBuilderExistingFields) -> Self::Output;
}

// It'd be nice for the compilation tests to live in tests/ with the rest, but short of pulling in
// some other test runner for that purpose (e.g. compiletest_rs), rustdoc compile_fail in this
// crate is all we can use.

#[doc(hidden)]
/// When a property is non-default, you can't ignore it:
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     x: i8,
/// }
///
/// let _ = Foo::builder().build();
/// ```
///
/// When a property is skipped, you can't set it:
/// (“method `y` not found for this”)
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(PartialEq, TypedBuilder)]
/// struct Foo {
///     #[builder(default, setter(skip))]
///     y: i8,
/// }
///
/// let _ = Foo::builder().y(1i8).build();
/// ```
///
/// But you can build a record:
///
/// ```
/// use typed_builder::TypedBuilder;
///
/// #[derive(PartialEq, TypedBuilder)]
/// struct Foo {
///     #[builder(default, setter(skip))]
///     y: i8,
/// }
///
/// let _ = Foo::builder().build();
/// ```
///
/// `skip` without `default` is disallowed:
/// (“error: #[builder(skip)] must be accompanied by default”)
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(PartialEq, TypedBuilder)]
/// struct Foo {
///     #[builder(setter(skip))]
///     y: i8,
/// }
/// ```
///
/// `clone` does not work if non-Clone fields have already been set
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(Default)]
/// struct Uncloneable;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     x: Uncloneable,
///     y: i32,
/// }
///
/// let _ = Foo::builder().x(Uncloneable).clone();
/// ```
///
/// Same, but with generics
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(Default)]
/// struct Uncloneable;
///
/// #[derive(TypedBuilder)]
/// struct Foo<T> {
///     x: T,
///     y: i32,
/// }
///
/// let _ = Foo::builder().x(Uncloneable).clone();
/// ```
///
/// Handling deprecated fields:
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[deprecated = "Don't use this!"]
///     #[allow(dead_code)]
///     value: i32,
/// }
///
/// #[deny(deprecated)]
/// Foo::builder().value(42).build();
/// ```
///
/// Handling invalid property for `strip_option`
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[builder(setter(strip_option(invalid_field = "should_fail")))]
///     value: Option<i32>,
/// }
/// ```
///
/// Handling multiple properties for `strip_option`
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[builder(setter(strip_option(fallback = value_opt, fallback = value_opt2)))]
///     value: Option<i32>,
/// }
/// ```
///
/// Handling alternative properties for `strip_option`
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[builder(setter(strip_option(type = value_opt, fallback = value_opt2)))]
///     value: Option<i32>,
/// }
/// ```
///
/// Handling invalid property for `strip_bool`
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[builder(setter(strip_bool(invalid_field = should_fail)))]
///     value: bool,
/// }
/// ```
///
/// Handling multiple propertes for `strip_bool`
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[builder(setter(strip_bool(fallback = value_bool, fallback = value_bool2)))]
///     value: bool,
/// }
/// ```
///
/// Handling alternative propertes for `strip_bool`
///
/// ```compile_fail
/// use typed_builder::TypedBuilder;
///
/// #[derive(TypedBuilder)]
/// struct Foo {
///     #[builder(setter(strip_bool(invalid = value_bool, fallback = value_bool2)))]
///     value: bool,
/// }
/// ```
fn _compile_fail_tests() {}
