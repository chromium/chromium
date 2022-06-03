# Frequently Asked Questions

[TOC]

## Usage

### How do I create a data file?
See the [`html_report` command docs](README.md#Usage_html_report).

### What do the different folder and file colors mean?
Containers (folders, files, and components) have different colors depending on
the symbols they contain. The color corresponds whatever symbol type has the
most bytes in that container. The color is the same as that symbol type's icon.

When hovering over the container, you can see a breakdown of all the symbol
types inside. The first row of that table indicates the symbol type with the
most bytes.

### What does "Type", "Count", "Total size", and "Percent" refer to?
When hovering over a container, a card appears breaking down the symbols stored
within that container. The data appears as a pie chart and a table with the
columns "Type", "Count", "Total size", and "Percent".

- **Type** refers to the symbol type that row represents.
- **Count** indicates the number of symbols of that type present in the
  container.
- **Total size** indicates how many bytes the symbols of that type take up in
  total.
- **Percent** indicates how much the total size of a symbol type takes up of the
  total size of the container. It also correlates to the pie chart sizes.

### Which keyboard shortcuts are supported?
Once the symbol tree is focused on, various keyboard shortcuts are supported
to navigate around the tree.

The symbol tree can be focused by clicking on it or by pressing _Tab_ until
the tree is focused.

Key | Function
--- | --------
_Enter_ or _Space_ | Open or close a container, just like clicking on it
↓ | Focus the node below the current node
↑ | Focus the node above the current node
→ | Move focus to the first child node, or open the current node if closed
← | Move focus to the parent node, or close the current node if open
_Home_ | Move focus to the topmost node
_End_ | Move focus to the bottommost node
_A-z_ | Focus the next node that starts with the given character
_*_ | Expand all sibling containers of the current node.

## Symbols

A description of how size information is collected, including descriptions of
each symbol type, is detailed in
[README.md](README.md#how-are-symbols-collected).

### What are "Other small" symbols for?
To reduce the size of the generated data file, small symbols are omitted by
default. Small symbols of the same type are combined into an "Other small
[type]" bucket.

More symbols can be displayed by using the `--all-symbols` flag
when generating the data file. However, the data file will be larger and will
take longer to load.

## Filters

### What regular expressions syntax is supported?
The contain and exclude regular expressions are evaluated against each symbol's:
* Full Name (as shown on the details card for it)
* Source Path
* Grouping (when a grouping is active).

The "Symbols must contain" filter is applied before the "Symbols must exclude"
filter.

Example filter | Regular expression
-------------- | ------------------
Find symbols in `MyJavaClass` | `^MyJavaClass#`
Find folders named `myfolder` | `^myfolder$`
