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
Dictionaries are moved outside the main interface to the top level of the file. Their internal structure remains the same. Descriptive comments above them should be moved along with them. For dictionary members, non-optional members must be prefixed with `required`, and optional members should not have a `?` suffix.
e.g.
```
dictionary MyDictionary {
  required DOMString requiredMember;
  DOMString optionalMember;
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
* `[requiredCallback]` Attribute: If the original callback was not optional, you must add the `[requiredCallback]` extended attribute before the `static Promise` return type.
* void Keyword: The `void` keyword should be replaced with `undefined`.

### Function Documentation Comments
The documentation comments for functions need to be updated.
* If there is a description for the old `|callback|` parameter should be changed to `|Returns|`, otherwise there is no need for a `|Returns|` description.
* A new `|PromiseValue|` comment should be added to document what the promise resolves to, using the name from the old callback parameter. For example, if the old callback was `void(boolean wasCleared)`, the new comment would be `|PromiseValue|: wasCleared`.
* If the callback definition also had a parameter comment describing it, it should be added to the `|PromiseValue|: name` comment separated with a colon+space. For example, if the old callback was:
```
// Description of the callback.
// |param| : A param description.
callback FunctionCallback = void(SomeType param);

...
// Description of the function.
// |callback| : Called when the function is done.
static void someFunction(FunctionCallback callback);
```
This would become:
```
// Description of the function.
// |Returns|: Called when the function is done.
// |PromiseValue|: param: A param description.
static Promise<SomeType> someFunction();
```
* Comments for functions can have both a `|Returns|` and a `|PromiseValue|`  if needed.

### Events
The old `interface Events { ... }` is replaced with a more explicit event handling interface. For each event, such as `onFoo`, follow these steps:
1. **Define a Listener Callback.** Create a new callback definition for the event listener. The name should be `OnFooListener`. If the argument passed to the callback had a comment description previously, that should be moved to the new callback definition; otherwise, it does not need a comment describing that argument.
```
// Fired when an alarm has elapsed. Useful for event pages.
// |alarm|: The alarm that has elapsed.
static void onAlarm(Alarm alarm);
```
Would become:

```
// Listener callback for the onAlarm event.
// |alarm|: The alarm that has elapsed.
callback OnAlarmListener = undefined (Alarm alarm);
```

2. **Define the Event Interface.** Create a new interface named `OnFooEvent` that inherits from `ExtensionEvent` and includes the standard `addListener`, `removeListener`, and `hasListener` methods. This interface should come directly after the corresponding callback definition.
```
interface OnAlarmEvent : ExtensionEvent {
  static undefined addListener(OnAlarmListener listener);
  static undefined removeListener(OnAlarmListener listener);
  static boolean hasListener(OnAlarmListener listener);
};
```

3. **Add Event Attribute.** In the main API interface (e.g., `Alarms`), add a `static attribute` for the event. If the original event definition had a general comment describing it, that should be moved above this new attribute.
```
// Fired when an alarm has elapsed. Useful for event pages.
static attribute OnAlarmEvent onAlarm;
```

## Canonical Examples
The `./test/converted_schemas/` folder contains examples of previous conversions that can be used as reference for other syntax and formatting. An example of the alarms conversion from that folder:

Old alarms IDL:
@./test/converted_schemas/alarms.idl

New alarms WebIDL:
@./test/converted_schemas/alarms.webidl
