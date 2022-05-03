Tests that RawSourceCode listeners count won't grow on each script pause. Bug 70996


Running: testCallStackPlacardsDiscarded
Script source was shown.
Set timer for test function.
Received DebuggerPaused event.
Function name: testFunction
Script execution paused.
Script execution resumed.
Set timer for test function.
Received DebuggerPaused event.
Function name: testFunction
Script execution paused.
Script execution resumed.

