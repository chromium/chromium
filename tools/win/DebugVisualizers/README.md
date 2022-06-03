This directory contains "natvis" (Native Visualization) files,
that provide custom views in the Visual Studio debugger.
The official Natvis documentation is located at
[Create custom views of C++ objects in the debugger using the Natvis
framework](https://docs.microsoft.com/en-us/visualstudio/debugger/create-custom-views-of-native-objects).

We have not found a good way to automate testing these files.
The following process is recommended before updating these files.

## Preparations

These files are embedded in `.lib`, `.dll`, or `.exe` so that the debugger can
use them without referring to the original `.natvis` files.

When you edit them, you can run a full build to update all of them, but
following steps will help to avoid the full build, and to benefit from Visual
Studio's dynamic reloading feature.

1. Comment out the `.natvis` you want to edit in `BUILD.gn`. You can find them
by [Chromium Code Search](https://source.chromium.org/search?q=.natvis%20file:BUILD.gn&ss=chromium).
2. Create a directory "`Visual Studio 2019\Visualizers`" in your Documents
folder if it does not exist.
3. Copy the `.natvis` files you want to edit to the directory.

With these steps done, if you save the `.natvis` files in your Documents folder
while you are debugging, Visual Studio will automatically reload them, run
diagnostic and show messages to Output window if any, and update
Auto/Local/Watch windows without stopping the current debug session.

## Diagnostic

1. Go to Tools - Options, Debugging, Output Window.
Select "Warning" for "Natvis diagnostic messages (C++ only)".

With this set, Visual Studio shows diagnostic warning and error messages to the
Output window when it reads `.natvis` files.

## Testing

It is recommended to do at least following tests before updating these files.

1. Enable the diagnostic warnings above.
2. Start the debugger and break somewhere where you can watch variables of the
types you're interested.
3. Check the Output window and verify that there are no errors or warnings for
the files you edit.
4. Check the types you modified are displayed correctly in Auto/Local/Watch
windows.
5. If the type has `Condition` or `IncludeView` attributes, they are like
"if"-statements. Please make sure that your test function exercises all
conditions and views.

If there is not a good test that uses the variables of interest then a test
should be created. It can be a test that does nothing except declare the
variables of interest (using base::debug::Alias to prevent them from being
optimized away). Whether it is a new or existing test it should be listed in the
commit message to aid reviewers in doing an actual verification that the changes
work. As an example, consider `TEST(ValuesTest, TakeList)` which is very helpful
for testing `base::Value visualizers`.