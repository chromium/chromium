# Containers

Containers are used for classes that will control multiple widgets. These
classes should listen to user events from the widgets and convert them to Files
app specific feature, usually by dispatching an Action to the Store.

Code in this folder can depend on other folders such as `../widgets`,
`../state/`, `../lib/`, etc.
