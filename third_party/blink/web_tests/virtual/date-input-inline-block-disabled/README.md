This is a virtual test suite for the behavior before http://crrev.com/c/2555698
which may be web incompatible. This virtual test suite makes sure that when the
corresponding flag is disabled, the old behavior still works as expected. When
the new beahvior has been fully launched without issues, then this virtual test
suite can be removed.

Flag: --disable-features=DateInputInlineBlock
Bug: crbug.com/970249
