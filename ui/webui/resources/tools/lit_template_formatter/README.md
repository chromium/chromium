# WebUI Lit Template Formatter

A Node.js-based formatter built specifically for Chromium
WebUI `CrLitElement` subclass template files (`.html.ts`).

This tool is designed to format these template files such that the HTML
template strings comply with the Chromium HTML style guide, while the surrounding
TypeScript is formatted by clang-format.

The tool is currently in beta, and is *not* run automatically via `git cl format`.
It is checked in to allow further development and to enable developers to run
the tool manually as desired to reduce time spent doing tedious HTML template
formatting.

## Known limitations

The tool is limited to files that have the following format:
```ts
getHtml(this: SomeCustomElement) {
  return html`
<!-- all HTML goes here -->
`;
}
```

Non-CrLitElement subclass templates and template files where `getHtml` does
not return a single tagged template literal created by `html` are not
supported. If executed against a non-CrLitElement subclass template, the tool
gracefully logs an info message and skips formatting. If executed against a
CrLitElement template that doesn't provide a single tagged template literal
an error is thrown. In either case the source file is left unmodified.

## How to Run

Invoke the tool directly via Node from the root Chromium checkout directory,
passing the desired target template file paths as arguments:

```bash
./third_party/node/linux/node-linux-x64/bin/node \
  ui/webui/resources/tools/lit_template_formatter/main.js <path/to/file.html.ts>
```

You can format multiple files or entire folders sequentially in a single run:
```bash
find chrome/browser/resources/pdf/elements -name "*.html.ts" | xargs \
  ./third_party/node/linux/node-linux-x64/bin/node \
  ui/webui/resources/tools/lit_template_formatter/main.js
```

## Core Pipeline Architecture

The main entrypoint script `main.js` invokes a series of helper scripts to
correctly format the HTML and the embedded TypeScript expressions:

1.  **Extraction (`process_lit_template_ts.js`)**: Uses the native TypeScript
Compiler API to accurately locate and extract the template payload returned
inside the `getHtml` function. Embedded TypeScript expressions (`${...}`) are
mapped to temporary placeholders (e.g., `lit-expr-placeholder-N`), and the
placeholder and the code it replaced are stored in a map. The resulting output
string is valid standard HTML.
2.  **AST Parsing (`format_html.js`)**: Preprocesses the HTML to substitute
restricted tags and camelCase Lit property bindings for placeholders and
generates a complete HTML Abstract Syntax Tree (AST) using `parse5`. Traverses
the tree to compute target nesting depths and inserts standardized line ending
and whitespace block tokens.
3.  **Expression Formatting (`format_expressions.js`)**: Delegates TypeScript
expression blocks to Chromium's `clang-format`. Applies dynamic line wrap
widths (`ColumnLimit`) based on target structural indentation levels.
4.  **Re-Serialization (`serialize_html.js`)**: Serializes the modified AST back
into an HTML template string. Elements exceeding line width bounds are
automatically wrapped with wrapped lines indented +4 from the first line.
5.  **Reconstruction (`main.js`)**: Formats the rest of the TypeScript file
with `clang-format` and then places the formatted HTML template string back
into the formatted file.

## Disabling Formatting for Selected Blocks

Developers can opt out of automated structural formatting for specific sections
by wrapping target blocks inside lit-template-format-off/on comments:

```html
<div>
  <!-- lit-template-format-off -->
  <span   class="legacy-spacing" >Maintains Source Layout Exactly</span>
  <!-- lit-template-format-on -->
</div>
```

## Testing

The entrypoint for the tests is `lit_template_formatter_test.py`. This script
runs end to end tests and also invokes the Node-based unit tests in
`lit_template_formatter/lit_template_formatter_test.js`. To run:

```bash
python3 ui/webui/resources/tools/lit_template_formatter_test.py
```
