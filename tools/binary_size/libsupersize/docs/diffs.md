# SuperSize Diffs

Diffs are represented by a `DeltaSizeInfo`, where each `DeltaSymbol` contains a
reference to a `before` and `after` symbol.

For new symbols, `before` is None, and for removed symbols, `after` is None.

## Diff Algorithm

The diffing algorithm is optimized for the common case of many unchanged
symbols.

1. Create a map of `$DIFF_KEY` -> `symbol` for all "before" symbols.
2. For all "after" symbols, find matches using the map.
3. Repeat steps 1 and 2 for all remaining symbols using different `$DIFF_KEYs`
4. Treat all unmatches symbols as added or removed.

`DeltaSymbols` for symbols that have not changed are created when diffing, but
discarded when writing `.sizediff` files (see [file_format.md](file_format.md)).

The `$DIFF_KEYs` are:

1. `DIFF_KEY=(container, section, full_name, path, s.size_without_padding)`
   * Ensures identical symbols are matched.
2. `DIFF_KEY=(container, section, full_name, path)`
   * Ensures identical symbols that have changed in size are matched.
3. `DIFF_KEY=(container, section, name, path)`
   * Match up symbols with the same name, but differing function signatures.
4. `DIFF_KEY=(container, section, full_name)`
   * Match symbols that have changed paths.
   * Does not often match moved Java files, since package names tend to change.

## Diffs and Padding

Since symbol padding tends to fluctuate from build to build, changes to padding
is not considered. Padding is removed from all symbols when diffing and
per-section cumulative change in padding is shown by a symbol called
`Overhead: aggregate padding of diff'ed symbol`.

## Diffs and Aliases

No special treatment is made for symbols aliases. If the number of symbols in an
alias group changes, a diff will show a size change for all symbols in the
group. In the past there was an attempt to have diffs exclude showing changes to
the number of aliases in a group, but it never worked reliably and added a lot
of complexity. E.g., A symbol from one alias group changes to belong to a
different alias group.
