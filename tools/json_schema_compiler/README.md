The JSON Schema Compiler is used by the chrome extensions system to generate
various necessary files from the "schemas" in extensions. These include
the C++ type representations for API types, string blobs for the files,
feature availability information, and more. You can read more about these
in our documentation on [schemas](//chrome/common/extensions/api/schemas.md) and
[features](chrome/common/extensions/api/_features.md).

Note that, despite its name, the JSON Schema Compiler does not just handle JSON
files -- it also handles files written in IDL syntax.

The main entry point for generation is `compiler.py`, which then uses different
"compilers" and "generators" to produce necessary types and files. These are
used in gn actions in various files in the codebase, such as those in
//chrome/common/extensions/api/ and //extensions/common/api.

For any questions, reach out to the
[OWNERS](//tools/json_schema_compiler/OWNERS).
