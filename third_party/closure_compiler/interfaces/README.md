## Creating or Updating an Interface

`~/chromium/src/tools/json_schema_compiler/compiler.py` is used to generate or update an existing interface.

Example: Run `~/chromium/src/tools/json_schema_compiler/compiler.py --root ~/chromium/src --namespace extensions --generator interface extensions/common/api/system_display.idl > ~/chromium/src/third_party/closure_compiler/interfaces/system_display_interface.js` to generate an up-to-date system_display_interface.