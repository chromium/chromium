The JSON Schema Compiler is used by the chrome extensions system to generate
various necessary files from the "schemas" in extensions. These include
the C++ type representations for API types, string blobs for the files,
feature availability information, and more. You can read more about these
in our documentation on [schemas](/chrome/common/extensions/api/schemas.md)
and [features](/chrome/common/extensions/api/_features.md).

Note that, despite its name, the JSON Schema Compiler does not just handle JSON
files -- it also handles files written in IDL syntax.

The main entry point for generation is `compiler.py`, which then uses different
"compilers" and "generators" to produce necessary types and files. These are
used in gn actions in various files in the codebase, such as those in
//chrome/common/extensions/api/ and //extensions/common/api.

The loading of the schema files by `compiler.py` differs slightly between [JSON
schema files](/tools/json_schema_compiler/json_schema.py) and [IDL schema files
](/tools/json_schema_compiler/idl_schema.py). For JSON schemas we first strip
the "//" style comments from the JSON file and then parse the rest using
standard python libraries.  The resulting python object is then sent to the
"compilers" and "generators". For IDL schemas the process is a bit more
complicated. First we load the schema file with an IDL parser, which returns an
AST (Abstract Syntax Tree) that represents the contents of the file. This AST is
then passed to a "processor" that creates a python object based on it, mimicking
the same structure as the output from JSON schema loading. The resulting python
object is then passed to the "compilers" and "generators", just like the JSON
schema path.

NOTE: Currently we use [an old IDL parser](/ppapi/generators/idl_parser.py),
which means that the current format of our IDL files is not up to date with
modern WebIDL. However https://crbug.com/340297705 is working on fixing that, by
integrating the Blink maintained [WebIDL parser](/tools/idl_parser/) and
creating a new "[processor](/tools/json_schema_compiler/web_idl_schema.py)" to
transform the output to the format we expect.

For any questions, reach out to the
[OWNERS](/tools/json_schema_compiler/OWNERS).
