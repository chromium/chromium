```console
$ 04_01_possible --help
A simple to use, efficient, and full-featured Command Line Argument Parser

Usage: 04_01_possible[EXE] <MODE>

Arguments:
  <MODE>  What mode to run the program in [possible values: fast, slow]

Options:
  -h, --help     Print help information
  -V, --version  Print version information

$ 04_01_possible fast
Hare

$ 04_01_possible slow
Tortoise

$ 04_01_possible medium
? failed
error: 'medium' isn't a valid value for '<MODE>'
  [possible values: fast, slow]

For more information try '--help'

```
