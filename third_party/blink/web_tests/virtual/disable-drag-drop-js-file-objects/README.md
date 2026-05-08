# disable-drag-drop-js-file-objects

This virtual suite runs drag-and-drop tests with the DragAndDropJSFileObjects
Blink runtime feature disabled.

It is used to verify the legacy fallback path where JS-constructed File objects
are transferred as text/plain filename data instead of binary file content.
