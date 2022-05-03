Failure: If the second search ends up finding the text in this line we didn't restart the search from the last active match: last_step.
Start: The first search should match this word: first_step.
Success: The second search should match this word: last_step. Subsequent searches should fail.
PASS testRunner.findString("first_", []) is true
PASS testRunner.findString("first_step", ["StartInSelection"]) is true
PASS testRunner.findString("last_step", []) is true
PASS testRunner.findString("last_step", []) is false
PASS testRunner.findString("last_step", ["WrapAround"]) is true
PASS successfullyParsed is true

TEST COMPLETE

