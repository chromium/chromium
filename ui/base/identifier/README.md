# Chromium Unique Identifiers

**Unique identifiers** are values which:
 - Are `constexpr`
 - Are opaque
 - Are atomic on any platform where `intptr_t` is atomic
 - Have a unique default/null/false value
 - Are comparable
 - Can be keys in collections like `std::set` and `std::map`
 - Have unique names that can be retrieved at runtime
 - Can be looked up by name

A unique identifier variable can be empty, or it can be assigned to an existing,
known identifier.

```cpp
  MyIdentifier id;  // Empty identifier.
  CHECK(!id);       // This is a valid check.
  id = kMyIdentifierValue;  // Assign an identifier constant.
  CHECK(!!id);      // Valid id is truthy.
  CHECK_NE(id, kMyOtherIdentifierValue);  // Id constants are distinct.
```

## Creating Unique Identifier Types

The underlying type of all identifiers is `UniqueIdentifier`. However you will
not directly use this class. Instead, you create a strongly-typed identifier to
use in your component or library:

```cpp
  DECLARE_UNIQUE_IDENTIFIER_TYPE(MyIdentifier);
  DECLARE_UNIQUE_IDENTIFIER_TYPE(YourIdentifier);

  MyIdentifier mine;     // This is now an instance of my identifier type.
  YourIdentifier yours;  // This is an instance of your identifier type.
  mine = yours;          // ERROR: these types are not compatible!
```

Create a new identifier type for each _kind of thing_ you want to identify.
For example, the interaction library defines:
 - `ElementIdentifier` for UI elements
 - `CustomElementEventType` for unique custom UI events
 - `UntypedStateIdentifier` for states that are tracked during tests

## Creating Identifiers

To create a unique identifier value, do one of the following:
 - Use an existing `DECLARE/DEFINE*UNIQUE_IDENTIFIER_VALUE` macros.
 - Create your own convenience macros that call these macros.

Using the existing macros:

```cpp
  // The identifier type needs to be declared somewhere.
  DECLARE_UNIQUE_IDENTIFIER_TYPE(MyIdentifier);

  // In a .h file where you want to create identifier values:
  DECLARE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyIdentifierValue1);
  DECLARE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyIdentifierValue2);

  // In the corresponding .cc file:
  DEFINE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyIdentifierValue1);
  DEFINE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyIdentifierValue2);
```

Creating your own convenience macros:

```cpp
  // The identifier type is typically declared in the same file as the
  // convenience macros.
  DECLARE_UNIQUE_IDENTIFIER_TYPE(MyIdentifier);

  #define DECLARE_MY_IDENTIFIER_VALUE(Name) \
      DECLARE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, Name)
  #define DEFINE_MY_IDENTIFIER_VALUE(Name) \
      DEFINE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, Name)

  // Then call your macros instead of the generic ones.

  // In a .h file where you want to create identifier values:
  DECLARE_MY_IDENTIFIER_VALUE(kMyIdentifierValue1);
  DECLARE_MY_IDENTIFIER_VALUE(kMyIdentifierValue2);

  // In the corresponding .cc file:
  DEFINE_MY_IDENTIFIER_VALUE(kMyIdentifierValue1);
  DEFINE_MY_IDENTIFIER_VALUE(kMyIdentifierValue2);
```

For an example of a full set of convenience macros, see
[ElementIdentifier](/ui/base/interaction/element_identifier.h).

### Options for Creating Identifier Values

The following macros/macro pairs are provided:

| Declaration (.h) | Definition (.cc) | Name | Usage |
| --- | --- | --- | --- |
| DECLARE_UNIQUE_IDENTIFIER_VALUE | DEFINE_UNIQUE_IDENTIFIER_VALUE | "Name" | For public values outside of a class |
| DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE | DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE | "Class::Name" | For values which are class members |
|                                 | DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE | "File::Line::Name" | In an anonymous namespace or inside a function |
|                                 | DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE | "File::Line::Name" | Use when calling from another macro |

When creating convenience macros, you **must** use
`DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE()` instead of
`DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE()` or else the file and line numbers will
be wrong and the names may not be unique.

Examples:
```cpp
  // In .h file:
  DECLARE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyPublicId);

  class MyClass {
   public:
    DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyClassId);

    void Func();
  };

  // In .cc file:
  namespace {
    DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyFileLocalId);
  }

  DEFINE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyPublicId);
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(MyClass, MyIdentifier, kMyClassId);

  void MyClass::Func() {
    DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyFunctionLocalId);
  }

```

### Using Identifier Values

Variables of an identifier type start with a null value, which is always the
default-constructed value of an identifier variable.

You should primarily assign identifiers from identifier values created using
the declaration macros (see above).

You prefer to pass identifiers by value instead of const reference, since the
size of an identifier and a reference are the same, and passing by value avoids
having to dereference memory.

If you do need to communicate an identifier across process boundaries (such as
to/from a WebUI), use `GetName()` and `FromName()`:

```cpp
  void MaybeHighlightElement(MyIdentifier id) {
    mojoRemote.HighlightElement(id.GetName());
  }

  void OnIdentifierSelectedFromRemote(std::string id_name) {
    MyIdentifier id = MyIdentifier::FromName(id_name);
    NotifyIdentifierSelected(id);
  }
```

Note that until you have called `GetName()` at least once for a particular
identifier value, it will not be retrievable via `FromName()`. This is because
the lookup is lazily created.

If you want to ensure that `FromName()` will work later in a class' lifespan,
you can force it to cache by calling `GetName()` in the constructor or during
initialization:

```cpp
  void Init() {
    for (auto id : kKnownIdentifiers) {
      id.GetName();  // Force-cache name for retrieval.
    }
  }
```

Because identifiers have predictable names, a WebUI can know the names of
relevant IDs ahead of time.

```cpp
  DECLARE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kKnownIdentifier);
```

```ts
  function report() {
    proxy.identifierSelected('kKnownIdentifier');
  }
```

Refer to the table above to see how identifiers are named. Note that LOCAL
identifiers cannot be used in this way as they do not have stable names.
You can use `IfChange`...`ThenChange` to ensure that names are kept consistent
between identifiers in different languages/parts of the codebase.

## Typed Identifiers

A **typed identifier** combines a compile-time type with a unique identifier.
Typed identifiers are used to create ways to store and look up data that are
guaranteed type-safe at compile time without having to add RTTI or dynamic
casting. (The objects are stored by their underlying unique identifier.)

[Unowned user data](/ui/base/unowned_user_data/README.md) makes heavy use of
typed identifiers, and is a good place to start if you want to understand how
they can be effectively used.

### Using Typed Identifiers

In order to use typed identifiers, you must first create your own [untyped]
unique identifier type. You can use the corresponding macros to declare specific
typed identifiers:

| Declaration (.h) | Definition (.cc) | Name | Usage |
| --- | --- | --- | --- |
| DECLARE_TYPED_IDENTIFIER_VALUE | DEFINE_TYPED_IDENTIFIER_VALUE | "Name" | For public values outside of a class |
| DECLARE_CLASS_TYPED_IDENTIFIER_VALUE | DEFINE_CLASS_TYPED_IDENTIFIER_VALUE | "Class::Name" | For values which are class members |
|                                 | DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE | "File::Line::Name" | In an anonymous namespace or inside a function |
|                                 | DEFINE_MACRO_LOCAL_TYPED_IDENTIFIER_VALUE | "File::Line::Name" | Use when calling from another macro |

Again, you can create your own convenience macros, and again, you must call
`DEFINE_MACRO_LOCAL_TYPED_IDENTIFIER_VALUE()` instead of
`DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE()` when calling from your own macros.

Example of using typed identifiers to store data. This approach is used in
various systems including `UnownedUserDataHost` and
`[Un]OwnedTypedDataCollection`.
```cpp
  // Class which stores literal values of various types.
  class MyDataCollection {
   public:
    // Note that types can be defined inside of classes.
    DEFINE_UNIQUE_IDENTIFIER_TYPE(MyUntypedIdentifier);

    // Define this for convenience.
    template<typename T>
    using MyTypedIdentifier = ui::TypedIdentifier<MyUntypedIdentifier, T>;

    // Can put limitations on what can be stored.
    template<typename T>
      requires !std::is_reference<T> && !std::is_pointer<T>
    void StoreData(MyTypedIdentifier<T> typed_id, T value);

    template<typename T>
    T RetrieveData(MyTypedIdentifier<T> typed_id);

   private:
    std::map<MyUntypedIdentifier, struct DataStorage> storage_;
  };

  // Using the collection class we created:
  DECLARE_TYPED_IDENTIFIER_VALUE(
      MyDataCollection::MyUntypedIdentifier, int, kMyIntValue);
  DECLARE_TYPED_IDENTIFIER_VALUE(
      MyDataCollection::MyUntypedIdentifier, std::string, kMyStringValue);

  MyDataCollection coll;
  coll.StoreData(kMyIntValue, 3);
  coll.StoreData(kMyStringValue, "foo");
  const auto result = coll.RetrieveData(kMyIntValue);
  CHECK_EQ(3, result);
```

Example of using typed identifiers to create objects; this approach is used in
e.g. the Kombucha `ObserveState` verb.
```cpp
  DEFINE_UNIQUE_IDENTIFIER_TYPE(MyUntypedIdentifier);
  template<typename T>
  using MyTypedIdentifier = ui::TypedIdentifier<MyUntypedIdentifier, T>;

  // Construct a polymorphic object from arguments.
  template<typename T, typename... Args>
    requires std::derived_from<T, BaseType>
  std::unique_ptr<BaseType> CreateInstance(MyTypedIdentifier<T> id,
                                           Args&& args...) {
    return std::make_unique<T>(std::forward<Args>(args)...);
  }
```

## Best Practices

1. Create a new identifier type for each conceptual type of thing you want to
   identify. Previously, we used `ElementIdentifier` for everything and it was
   both very confusing and easy to cross-contaminate or misuse an identifier for
   the wrong thing.

2. Have a naming convention, especially for identifiers in the global scope.
   For example, in
   [browser_element_identifiers](/chrome/browser/ui/browser_element_identifiers.h)
   all of the values for identifiers end with "ElementId". This avoids potential
   name collisions.

3. Use `DECLARE/DEFINE_CLASS_` and `DEFINE_LOCAL_` macros wherever possible,
   with the latter being very useful for creating values in tests. This
   completely avoids potential name collisions, as any number of local ids in
   different files can have the same name without problems.

4. Identifiers are value types and should be passed by value when possible.
   Passing them by const reference is inefficient. You can pass an identifier
   variable by non-const reference or pointer if you intend for it to be an
   in/out parameter to the function.

```cpp
  void Func(MyIdentifier id);            // Good.
  bool Func(MyIdentifier& out_id, ...);  // Fine.
  void Func(const MyIdentifier& id);     // Unnecessary (but not incorrect).
```

5. Since identifiers have a natural null value (the default-constructed value)
   and are truthy/falsy based on whether they are null, there is rarely a need
   to wrap one in a `std::optional`.

```cpp
  // Good:
  MyIdentifier GetIdentifier();
  if (const auto my_id = GetIdentifier()) {
    // my_id is guaranteed to be non-null.
  }

  // Unnecessary and maybe wrong unless you intended to have two null values:
  std::optional<MyIdentifier> GetIdentifier();
  if (const auto my_id = GetIdentifier(); my_id.has_value()) {
    // my_id.value() may still be null/falsy!
  }
```

6. Do not rely on sort order of specific identifiers (e.g. in `std::set` and
   `std::map`) remaining stable across different invocations of the process.
   Ordering is based on memory addresses of statically-allocated data, but
   across different builds or even invocations this ordering might vary.
   - For example, it's safe to add `kMyIdentifier1` and `kMyIdentifier2` as keys
     in a map, but it's not safe to assume `kMyIdentifier1` will always sort
     before `kMyIdentifier2` every time your program is run.

7. Never use a `DEFINE_...` macro in a .h file.
   - Due to the implementation, this may result in the identifier having
     different values in different compilation units, which can lead to errors
     and even crashes.
   - This means you should generally avoid declaring unique IDs in template
     classes.

## Implementation Details

All unique identifiers - typed or not - contain exactly one data member, which
is a `const` pointer to a `UniqueIdentifierProvider`. `UniqueIdentifierProvider` is a
struct which:
 - Is only statically-allocated so its address is fixed and permanent.
 - Is only created via the `DEFINE*UNIQUE_IDENTIFIER_VALUE` macros.
 - Contains a single member which is a pointer to its name (also `constexpr`).

Therefore, the raw value of any identifier is the address of an impl struct, or
null for invalid/default-constructed identifiers. This address is used to get
the name corresponding to the identifier.

Names are cached in a global `NoDestructor` map lazily when they are retrieved,
allowing subsequent lookup by name. This avoids process load initialization. The
size of the map does not grow monotonically since identifiers can only be
created at compile time, so there are always O(1) possible entries.

Each macro also creates one `constexpr` identifier of the given type with the
specified name, which is linked to its corresponding impl. The name attached to
the impl corresponds to the name of the constant in a predictable way as per the
table above. (Note: exported identifiers are merely `const`.)

For example:
```cpp
  DEFINE_UNIQUE_IDENTIFIER_TYPE(MyIdentifier);

  // Creates `constexpr MyIdentifier kMyId` in the current namespace:
  DECLARE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyId);
  // Implements a `const internal::UniqueIdentifierProvider` with name "kMyId".
  DEFINE_UNIQUE_IDENTIFIER_VALUE(MyIdentifier, kMyId);
```

## Known Issues / Future Work

### Template Classes Cannot Easily Contain Identifier Values

Because the `UniqueIdentifierProvider` associated with an identifier must have a
stable address process-wide, they cannot be defined in .h files, lest different
compilation units/modules generate different instances and use different
underlying memory address values.

This can, in turn, lead to cases where two values that should be the same but
don't compare as equal, and - worst-case - to name collisions in the lookup that
will crash the program.

There is no easy workaround for this limitation.

### Avoiding Name Collisions Between Identifier Types

Currently, the names for identifiers of different types all go into the same
lookup table. This means that if you have two different identifier types, and
each declares an identifier called "kId", then the names will conflict.

_This is almost certainly bad code_, for reasons set out in the Best Practices
section above. However, as usage of unique identifiers increases, collisions
become more and more likely.

One option would be to prefix each name with the identifier's type (e.g.
"ElementIdentifier:kBrowserElementId").
 - Upsides:
   - Very readable. We already debug-print identifiers like this.
   - We already use prefixes for class and local identifiers.
   - This makes the type of identifier explicit in e.g. TypeScript, where only
     names are used.
 - Downsides:
   - Makes all of the names longer and easier to mess up when typing.
   - Makes lookups slightly slower.

Another option would be to create a separate lookup for each identifier type,
but that introduces a few additional issues like:
 - Ensuring the lookup maps are allocated, since the system relies on templates.
 - Not being able to look up raw `UniqueIdentifier` values by name.

Regardless of which approach is taken, it could potentially allow for run-time
type-checking when retrieving an identifier by name.
