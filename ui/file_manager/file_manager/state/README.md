# Files app state management

In this folder contains the data model and the APIs to manage the state for the
whole Files app.

## Deps

**Allowed:** This folder can make use of standard Web APIs and private APIs.

**Disallowed:** This folder shouldn't depend on the UI layer, like `widgets/`.

## Store

The `Store` is central place to host the whole app state and to subscribe/listen
to updates to the state.

The `Store` is a singleton shared throughout the app.

The `Store` class in `store.ts` is merely enforcing the data type specific to
Files app.  The store implementation itself in in `lib/base_store.ts`.

## Action

An `Action` is a request to change the app's state.

At its core, a `Action` is just a plain object, with a `type` property to
uniquely identify the type of that `Action`. We use Typescript typing to enforce
that shape and types in any `Action` object.

For example, a `Action` to change the current directory might look like:
```js
{
  type: 'CHANGE_DIRECTORY',
  newDirectory: '-- URL to the directory --',
}
```
## Reducers

Reducers are functions that take the current state and an `Action` and performs
the actual change to the state and returns the new state.

## State

The interface `State` in `state.ts` describe the shape and types of the Files
app state.

At runtime, the state is just a plain Object the interface is used throughout
the code to guarantee that all the parts are managing the plain Object with
correct types.
