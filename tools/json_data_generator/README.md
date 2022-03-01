# JSON Data Generator

This GN template will help you share JSON data across multiple languages (e.g
javascript, c++, etc.), it uses whatever JSON files (`*.json5`) you provided as
the data sources and generates whatever files you like based on the template
files (`*.jinja`) you provided.

## When to use?

If you see a comment in a C++ file saying "the data change here should be
reflected in another file xxx.js", then it's a good chance the shared data can
be extracted to a standalone JSON file to keep consistency and make it easier to
maintain.

## Get Started

Simply import the GN template file in your own GN file and pass your JSON data
source files and template files.

```
import("//tools/json_data_generator/json_data_generator.gni")

json_data_generator("my_json_data") {
  sources = [ "my_json_data.json5" ]
  templates = [
    "my_json_data.cc.jinja",
    "my_json_data.js.jinja",
  ]
  output_dir = "$root_gen_dir/my_dir"
}

```

The above GN build task will generate 2 files (given your are building for
`out/Default`): * `out/Default/gen/my_dir/my_json_data.cc` *
`out/Default/gen/my_dir/my_json_data.js`

## Available variables

The GN temple supports the following variables:

| Variable Name     | Description                                  | Mandatory |
| ----------------- | -------------------------------------------- | --------- |
| `sources`         | An array to include all the JSON data source | Yes       |
:                   : file paths.                                  :           :
| `templates`       | An array to include all the template file    | Yes       |
:                   : paths.                                       :           :
| `template_helper` | A string represents the template helper file | No        |
:                   : path.                                        :           :
| `output_dir`      | A string to indicate the output directory    | No        |
:                   : for the generated files. The default value   :           :
:                   : for this is `$target_gen_dir`, which is the  :           :
:                   : same directory your GN file stays.           :           :
| `deps`            | Same as the standard `deps` array.           | No        |

## Available `globals` in Jinja templates

The json data will be stored under a key in the template `model`, the key is the
same as the JSON file name.

```json
// my_data1.json5
{
  "a": 11
}
// my_data2.json5
[
  22, 33
]
```

For example, with the above 2 JSON files as the data sources, we can use these
expressions in the Jinja template files:

*   `{{ model.my_data1.a }}` will output `11`.
*   `{{ model.my_data2[1] }}` will output `33`.

Beside the data itself, there are also some other useful globals available:

*   `source_json_files` will give you the JSON paths array (same as `sources`).
*   `out_file_path` will give you the full path of this generated file.

Check `GetGlobals()` in [generator.py](./generator.py) to see all available
globals.

## Available `filters` in Jinja templates

*   `to_header_guard` will convert the string path you provide to the upper case
    with underscore style, which is quite handy if you are generate C++ header
    files.

Check `GetFilters()` in [generator.py](./generator.py) to see all available
globals.

## Provide your own custom `globals/filters`

> Check [test/jinja_helper.py](./test/jinja_helper.py) for more examples.

Jinja template language only supports very limited filters and python
expressions, if you need specific logic to handle your JSON data, you can pass
an additional python file as `template_helper`, the only requirement for the
file is it must include 2 functions:

```python
def get_custom_globals(model):
    return {
        # custom globals
    }
}

def get_custom_filters(model):
    return {
        # custom filters
    }
}
```

This file will be processed when rendering your template, the loaded JSON data
will be passed in as the `model` parameter, so you can do whatever logic you
like here and use your custom globals/filters in your template file.
