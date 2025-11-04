# Extensions WebIDL Conversion

This document describes the process for converting old style Extensions IDL schema files to modern WebIDL syntax and details rules for how to handle specific conversion cases.

## Motivation/Background
For a long time Extension API schema files have used an old unmaintained IDL parser, which has fallen out of date with modern WebIDL. Recently support for a modern Blink maintained WebIDL parser was added (crbug.com/340297705), which means we are now able to start updating our IDL schema files to modern WebIDL.

## Conversion Process
When converting an IDL file a new file should be created in the same location as the old schema with the same name but using a `.webidl` suffix instead.
Once the converted file has been created, it must be verified to have no differences from the output of the original by running the `tools/json_schema_compiler/web_idl_diff_tool.py` utility, which takes the filename of the old and the new schema as arguments.
e.g. `tools/json_schema_compiler/web_idl_diff_tool.py extensions/common/api/alarms.idl extensions/common/api/alarms.webidl`
Only after it is verified that there is no differences by the tool outputting `No difference found!`, the old `.idl` schema file can be moved to `tools/json_schema_compiler/test/converted_schemas/` along with a copy of the newly converted `.webidl` schema. Then `tools/json_schema_compiler/web_idl_diff_tool_test.py` should be updated to add these filenames to the `converted_schemas` array and `web_idl_diff_tool_test.py` should be run to verify it passes with the newly added entry.
Finally, the associated `schema.gni` file with the old `.idl` filename will need to be updated to `.webidl`.

## Conversion Rules
The following conversion rules and the provided `alarms` API conversion example can be used as a guide for structure, syntax, and style.

### Namespace Removal
The top-level `namespace` declaration is removed.
* The content of the old `namespace foo { ... }` becomes the new top-level `interface Foo { ... }`. Note the capitalization change from `foo` to `Foo`.
* The descriptive comment above the old namespace should be moved to be above the new interface definition.
* A `partial interface Browser` is added at the end of the file to expose the API on the `browser` object. For an API named `foo`, this should be:
```
interface Foo {
  ...
}

partial interface Browser {
  static attribute Foo foo;
};
```
* If the previous namespace name contained periods, it will need to use nested partial interfaces for each pieces of the API name. For example, an API which was previously exposed under `system.cpu` would become:
```
// Note the interface name here is only the final part of the API name.
interface CPU {
  ...
}

partial interface System {
  static attribute CPU cpu;
}

partial interface Browser {
  static attribute System system;
}
```

### Dictionaries
Dictionaries are moved outside the main interface to the top level of the file. Their internal structure remains the same. Descriptive comments above them should be moved along with them.

A key difference between the old and new formats is the default for dictionary members:
*   The old `.idl` format treated members as **required by default**. A `?` suffix was used to mark a member as optional.
*   The new WebIDL format treats members as **optional by default**. The `required` keyword is used to mark a member as non-optional.

Therefore, the conversion rule is:
*   If a member in the old `.idl` file **does not** have a `?` suffix, it was required and **must** be prefixed with `required` in the new `.webidl` file.
*   If a member in the old `.idl` file **does** have a `?` suffix, it was optional and **must not** have a `required` prefix in the new `.webidl` file.

**Example of Conversion:**

**Before (`.idl`):**
```
dictionary MyInfo {
  // This is required by default in the old format.
  DOMString name;

  // The '?' makes this optional.
  long? age;
};
```

**After (`.webidl`):**
```
dictionary MyInfo {
  // Must be explicitly marked 'required' in the new format.
  required DOMString name;

  // Is optional by default in the new format, so no keyword is needed.
  long age;
};
```

### Operations on Dictionaries to Callbacks
Some dictionaries previously would define static operations on them to indicate they had a callable function. These are now instead defined using callbacks, which are defined on the top level of the IDL file and then referenced as the type for the dictionary member. Any descriptive comment above the old operation definition should remain above the new dictionary member that replaced it.
e.g.
```
dictionary AutomationEvent {
  // Function description.
  static void stopPropagation();
};
```
Would become:
```
callback StopPropagationCallback = undefined();
dictionary AutomationEvent {
  // Function description.
  required StopPropagationCallback stopPropagation;
};
```

### Enums
Enums are moved outside the main interface to the top level of the file. Their internal structure remains very similar, but the values are instead defined with quoted strings e.g.
```
enum VendorIdSource {
  "bluetooth",
  "usb"
};
```
Each enum value should be put on a new line. Descriptive comments above the whole enum should be moved along with them.

### Functions and Callbacks to Promises
All functions that used a trailing callback must be converted to return a `Promise`.
* Return Type: The function's return type changes from `static void` to `static Promise<T>`.
* Callback Removal: The final callback parameter is removed from the function's signature.
* Promise Type T: The type T inside the `Promise<T>` is determined by the arguments of the old callback:
  * A callback with one argument `void(Type arg)` becomes `Promise<Type>`. For example:
    * `void(Alarm alarm)` becomes `Promise<Alarm>`.
    * `void(optional DOMString foo` becomes `Promise<DOMString?>`.
  * A callback with no arguments `void()` becomes `Promise<undefined>`.
  * A nullable argument `void(optional Type arg)` becomes a nullable promise type `Promise<Type?>`.
  * An array argument `void(Type[] arg)` becomes a sequence `Promise<sequence<Type>>`.
* Callback Optionality: If the original callback was not marked as `optional`, the new definition must have a `[requiredCallback]` extended attribute added before the `static Promise` return type. e.g. `[requiredCallback] Promise<boolean> checkFoo();`
* void Keyword: The `void` keyword should be replaced with `undefined`.

### Promise Function Documentation

When a function is converted from using a callback to returning a `Promise`, its documentation must be updated to reflect this change. The single description for the old `|callback|` parameter is split into two new, more specific tags: `|Returns|` and `|PromiseValue|`.

1.  **The `|Returns|` Tag: What the Function Returns**
    This tag describes the `Promise` object itself. Its text should be the original description of the `|callback|` parameter from the function's main documentation block. It explains *when* the promise will be resolved.

    *   **Example Source:** `// |callback|: Called with the resulting alarm, if any.`
    *   **Becomes:** `// |Returns|: Called with the resulting alarm, if any.`

2.  **The `|PromiseValue|` Tag: What the Promise Resolves With**
    This tag describes the value *inside* the `Promise` upon successful resolution. To find this text, look at the original `callback` definition (e.g., `callback AlarmCallback = ...`). The documentation for the parameters passed to that callback becomes the content for the new `|PromiseValue|` tag.

    *   **Example Source:**
        ```
        // |alarm|: The alarm that was found.
        callback AlarmCallback = void(optional Alarm alarm);
        ```
    *   **Becomes:** `// |PromiseValue|: alarm: The alarm that was found.`

If the original callback took no arguments (e.g., `void()`), then no `|PromiseValue|` tag is needed.

**Complete Example:**

**Before:**
```
// Description of the function.
// |name|: The name of the alarm to get. Defaults to the empty string.
// |callback|: Called with the resulting alarm, if any.
static void get(optional DOMString name, AlarmCallback callback);

// |alarm|: The alarm that was found.
callback AlarmCallback = void(optional Alarm alarm);
```

**After:**
```
// Description of the function.
// |name|: The name of the alarm to get. Defaults to the empty string.
// |Returns|: Called with the resulting alarm, if any.
// |PromiseValue|: alarm: The alarm that was found.
[requiredCallback] static Promise<Alarm?> get(optional DOMString name);
```

### Events
The old `interface Events { ... }` is replaced with a more explicit event handling interface. For each event, such as `onFoo`, follow these steps:
1. **Define a Listener Callback.** Create a new callback definition for the event listener. The name should be `OnFooListener`. If the argument passed to the callback had a comment description previously, that should be moved to be directly above the new callback definition; otherwise, it does not need a comment describing that argument. **Important:** The descriptive comment for each argument in the old event (e.g., `// |alarm|: The alarm that has elapsed.`) **must** be moved to be directly above the new `callback` definition.

For example, an event with multiple parameters would be converted as follows:

**Before:**
```
// Old .idl file
interface Events {
  // Fired when something interesting happens.
  // |param1|: The first parameter.
  // |param2|: The second parameter.
  static void onFoo(DOMString param1, long param2);
};
```

**After:**
```
// New .webidl file

// |param1|: The first parameter.
// |param2|: The second parameter.
callback OnFooListener = undefined (DOMString param1, long param2);
```

2. **Define the Event Interface.** Create a new interface named `OnFooEvent` that inherits from `ExtensionEvent` and includes the standard `addListener`, `removeListener`, and `hasListener` methods. This interface should come directly after the corresponding callback definition.
```
interface OnFooEvent : ExtensionEvent {
  static undefined addListener(OnFooListener listener);
  static undefined removeListener(OnFooListener listener);
  static boolean hasListener(OnFooListener listener);
};
```

3. **Add Event Attribute.** In the main API interface (e.g., `Alarms`), add a `static attribute` for the event. The general descriptive comment for the event (e.g., "Fired when...") should be moved above this new attribute.
```
// Fired when something interesting happens.
static attribute OnFooEvent onFoo;
```

## Canonical Examples
The `./test/converted_schemas/` folder contains examples of previous conversions that can be used as reference for other syntax and formatting. An example of the alarms conversion from that folder:

Old alarms IDL:
@./test/converted_schemas/alarms.idl

New alarms WebIDL:
@./test/converted_schemas/alarms.webidl
