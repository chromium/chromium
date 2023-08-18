This virtual test suite tests the old behavior for the rendering of select
elements with the option attribute while in quirks mode. I am trying to remove
the quirks mode behavior, but in case it isn't web compatible, we will have to
go back to the behavior tested by this virtual test suite.

Flag: --disable-features=OptionElementLabelQuirk
Bug: crbug.com/1403735
