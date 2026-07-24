Tests with the TableCellBorderColorInherit feature disabled.

This feature (enabled by default) makes interior table cell borders created by
the `border`/`rules` attributes inherit the table's border-color. With it
disabled, those borders resolve to `currentColor` instead, since border-color
is not an inherited property per CSS Backgrounds 3. See crbug.com/40745409.
