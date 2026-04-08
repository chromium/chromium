macro_rules! formatting_docs {($($additional_fmt_overrides:expr)?) => {
concat!(r##"
# Formatting

Literals are Display formatted by default, so that you can pass string literals 
without worrying about what the current formatting settings are.

Expressions are formatted as determined by the `$fmtarg` argument.

Note that literals inside parentheses (eg: `(100)`) are considered expressions
by this macro.

### Formatting overrides

You can override how an argument is formatted by prefixing the argument expression with 
any of the options below:
- `debug:` or `{?}:`: `Debug` formats the argument.
- `alt_debug:` or `{#?}:`: alternate-`Debug` formats the argument.
- `display:` or `{}:`: `Display` formats the argument.
- `alt_display:` or `{#}:`: alternate-`Display` formats the argument.
- `bin:` or `{b}:`: `Debug` formats the argument, with binary-formatted numbers.
- `alt_bin:` or `{#b}:`:
alternate-`Debug` formats the argument, with binary-formatted numbers.
- `hex:` or `{X}:`:
`Debug` formats the argument, with hexadecimal-formatted numbers.
- `alt_hex:` or `{#X}:`:
alternate-`Debug` formats the argument, with hexadecimal-formatted numbers.
"##,
$($additional_fmt_overrides,)?
r##"
### String formatting

String expressions are debug-formatted like this:
- Prepending and appending the double quote character (`"`).
- Escaping the `'\t'`,`'\n'`,`'\r'`,`'\\'`, `'\''`, and`'\"'` characters.
- Escaping control characters with `\xYY`, 
where `YY` is the hexadecimal value of the control character.

"##
)}}

macro_rules! limitation_docs {() => {
"
Arguments to the formatting/panicking macros must have a fully inferred concrete type, 
because `const_panic` macros use duck typing to call methods on those arguments.

One effect of that limitation is that you will have to pass suffixed 
integer literals (eg: `100u8`) when those integers aren't inferred to be a concrete type.
"
}}
pub(crate) use limitation_docs;
