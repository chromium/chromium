# Files app state management

This folder contains the data model and the APIs to manage the state for the
whole Files app.

## Deps

**Allowed:** This folder can make use of standard Web APIs and private APIs.

**Disallowed:** This folder shouldn't depend on the UI layer, like `widgets/`.

## Debug

Run `fileManager.store_.setDebug(true)` in the DevTools console to enable store debugging,
which will show verbose logs for each action dispatched and state change, it
will also show each selector emitted.

Run `fileManager.store_.setDebug(false)` to turn it off.

## Store

The `Store` is central place to host the whole app state and to subscribe/listen
to updates to the state.

The `Store` is a singleton shared throughout the app.

The `Store` class in `store.ts` is merely enforcing the data type specific to
Files app. The store implementation itself is in `lib/base_store.ts`.

The store requires an initial state and a collection of slices to be constructed.

## Slice

Slices are instances of the class `Slice`, each representing an entry directly
under the root state object. For example, if the store's state shape is:

```ts
{
  cat: CatData;
  dog: DogData;
}
```

Then one should expect this store to be constructed with a cat and a dog slice,
each focused on its own slice of the store.

Slices are instantiated with the shape of the store and their own shape, specified
through templates. Additionally, they take a name (which should match the key for
its entry under the root state), which is mostly used to automatically prefix
their actions for debugging. For example, a slice might be instantiated as such:

```ts
new Slice<State, State["search"]>("search");
```

Each slice holds a collection of reducers related to the shape of the state they own.

When reducers are added to slices, they return action factories that can be used for:

1. Dispatching the action to the store; and
2. Registering reducers in other slices with the same action type.

## Ducks

Ducks is a pattern of organizing parts of the store used by Files app.

The idea behind ducks is relatively simple: collocate actions, action factories,
reducers, selectors, and action producers as much as possible according to the
slice of the store they are more closely related to.

In practice, this translates into creating one file per `Slice` instance and
grouping all those files under a `ducks/` directory.

## Action

An `Action` is an event that gets captured by one or more reducers registered
in the store and contains the necessary information to update the app's state.

At its core, an `Action` is just a plain object, with a `type` property to
uniquely identify the type of that `Action`. We enforce the shape of actions
by creating action factories through the use of `Slice::addReducer()`.

For example, an `Action` to change the current directory might look like:

```js
{
  type: 'CHANGE_DIRECTORY',
  payload: {
    newDirectory: '-- URL to the directory --',
  },
}
```

### Naming actions

When naming actions, it is a good idea to think of them as events rather than
commands or setters. Remember that multiple reducers can intercept and handle
the same action. Naming actions as events makes them more scalable as we're not
tying a specific behavior to their name.

Further reading:

https://redux.js.org/style-guide/#model-actions-as-events-not-setters

## Action factory

Action factories are callable objects returned by `Slice::addReducer()`.

Action factories can be used to dispatch actions of the type specified by
`Slice::addReducer`, e.g.:

```ts
store.dispatch(updateDeviceConnectionState());
```

They can also be used to register reducers for the same action type in other
slices, e.g.:

In `ducks/device.ts`:
```ts
export const updateDeviceConnectionState = slice.addReducer(
  'set-connection-state',
  updateDeviceConnectionStateReducer
);

function updateDeviceConnectionStateReducer(
  currentState: State,
  payload: {
    connection: chrome.fileManagerPrivate.DeviceConnectionState;
  }
): State {
  // ...
}
```

In `ducks/volume.ts`:
```ts
slice.addReducer(
  updateDeviceConnectionState.type,
  updateDeviceConnectionStateReducer
);

function updateDeviceConnectionStateReducer(
  currentState: State,
  payload: GetActionFactoryPayload<typeof updateDeviceConnectionState>
): State {
  // ...
}
```

### Raw actions vs action factories

Avoid creating actions directly and instead use action factories to ensure
your dispatched actions are always correctly typed.

## Reducers

Reducers are functions that take the current state and an `Action` and perform
the actual change to the current state, returning a new state (never mutating
the state object it receives).

## Actions Producer

Actions Producers (AP) are async generator functions, that call asynchronous
APIs and yield Actions to the Store.

Each AP should be wrapped in a concurrency model to guarantee that the execution
is still valid in-between each async API call.

See more details in the internal design doc:
http://go/files-app-store-async-concurrency

## Selectors

Selectors are functions that efficiently notify parts of the app about specific
changes in the state tree. In other words, they "select" a part of the state
tree and provide non-redundant updates about changes to that part.

Both the store and slices come with default selectors that are available
immediately after they are instantiated. The store's default selector (the root
selector) can be used to get updates on changes to the root state object, and the
slice default selectors on changes to the respective slice of the store they
represent.

### Combining selectors

Selectors can be combined using the `combineXSelectors()` functions, which work by
taking a group of existing selectors and a "select" function that combines their
outputs into a new output of interest. For example:

```ts
const newSelector = combine2Selectors(
      (state, numVisitors) => state.something + numVisitors,
      store.selector, numVisitorsSlice.selector);
```

Here, `2` in `combine2Selectors` refers to the number of input selectors used to
create the new selector. Notice that the number of input selectors and the number
of arguments passed to the "select" function should be the same.

### General usage

To subscribe to new values emitted by a selector, call:

```ts
selector.subscribe(callback);
```

To get the last value emitted by a selector, call:

```ts
selector.get();
```

### Usage with Lit elements

Selectors can be used in Lit components using the selector's `createController()`
helper function. The created controller takes a reference to the component which
is then used to automatically re-render the component whenever the selector
emits a new value. It also becomes responsible for automatically unsubscribing
from the selector when the component is destroyed. For example:

```ts
  @customElement('xf-test')
  class XfTest extends XfBase {
    testCtrl = testSelector.createController(this);

    override render() {
      return html`
      <div id="test">${this.testCtrl.value}</div>
    `;
    }
  }
```

## State

The interface `State` in `../state/state.js` describes the shape and types
of the Files app state.

At runtime, the state is just a plain Object the interface is used throughout
the code to guarantee that all the parts are managing the plain Object with
correct types.

## Adding new State

Every part of the State should be typed. While we still support Closure type
check we maintain the type definition using `@typedef` in
`../state/state.js` which is understood by both Closure and TS compilers.

**NOTE:** To define a property as optional you should use the markup
`(!TypeName|undefined)`, because that's closest we can get for Closure and TS to
understand the property as optional, TS still enforces the property to be
explicitly assigned `undefined`.

The global State type is named **`State`**. You can add a new property to
`State` or any of its sub-properties.

In this hypothetical example let's create a new feature to allow the user to
save their UI settings (sort order and preferred view (list/grid)).

```javascript
/** @enum {string} */
export const SortOrder = {
  ASC: 'ASCENDING',
  DESC: 'DESCENDING',
};

/** @enum {string} */
export const ColumnName = {
  NAME: 'NAME',
  SIZE: 'SIZE',
  DATE: 'DATE',
  TYPE: 'TYPE',
};

/**
 * @typedef {{
 *   order: !SortOrder,
 *   column: !ColumnName,
 * }}
 */
export let Sorting;

/**
 * Types of the view for the right-hand side of the Files app.
 * @enum {string }
 */
export const ViewType = {
  LIST: 'LIST-VIEW',
  GRID: 'GRID-VIEW',
}

/**
 * User's UI Settings.
 * @typedef {{
 *   sort: !Sorting,
 *   detailView: !ViewType,
 * }}
 */
export let UiSettings;

/**
 * @typedef {{
 *   ...
 *   uiSettings: !UiSettings,
 * }}
 */
export let State;
```

## Adding new Slice

Actions are create by registering a new reducer to a slice via
`Slice::addReducer()`. This guarantees that all actions are assigned to at least
one reducer.

Let's continue the example of the UI Settings and create a slice with actions
and reducers to update the `uiSettings` state:

```ts
const slice = new Slice<State, State['uiSettings']>('uiSettings');
export {slice as uiSettingsSlice};

export const changeSortingOrder = slice.addReducer('change-sort-order', (state: State, payload: {
    order: SortOrder,
    column: ColumnName,
  }) => ({
    ...state,
    uiSettings: {
      ...uiSettings,
      sort: {
        order: payload.order,
        column: payload.column,
      },
    },
  }));

export const switchDetailView = slice.addReducer('switch-detail-view', (state: State) => ({
    ...state,
    uiSettings: {
      ...uiSettings,
      detailView: state.uiSettings.detailView === ViewType.LIST ? ViewType.GRID :
                                                                  ViewType.LIST,
    },
  }));
```

In `./store.ts` add the default empty state for the new data.

```typescript
export function getEmptyState(): State {
  return {
    ...
    uiSettings: {
      sort: {
        order: SortOrder.ASC,
        column: ColumnName.NAME,
      },
      detailView: ViewType.LIST,
    },
  };
}
```

Register the new slice `ui_settings.ts` in `../file_names.gni`.

```python
ts_files = [
   ...
  "file_manager/state/ducks/ui_settings.ts",
]
```

Use the new actions in a container.

For example a hypothetical "toolbar_container.ts" handling the "switch-view"
button and the "sort-button".

```typescript
import {switchDetailView} from '../state/ducks/ui_settings.js';

class ToolbarContainer {
  onSwitchButtonClick_(event) {
    this.store_.dispatch(switchDetailView());
  }

  onSortButtonClick_(event) {
    // Figure out the sorting order from the event.
    const sortOrder = event.target.dataset.order === 'descending' ?
        SortOrder.DESC :
        SortOrder.ASC;

    this.store_.dispatch(changeSortOrder(order: sortOrder));
  }
}
```

## Add new Actions Producer

Continuing with the UI Settings state example, let's change the feature to
persist this data to localStorage.

For Actions Producer (AP), we have to always think on the concurrency model that
the feature requires.

For UI Settings we have 2 actions:
1. Switch view: Which depends on the previous state to flip to a new state. So
   the Queue/Serialize model is a good fit.
1. Change Sort Order: The Keep Latest is a good fit, because intermediary sort
   order aren't relevant, only the last option made by the user matters.

The State changes slightly from before, we add a `status` attribute, its type is
the enum `PropStatus`.

In file `../state/state.js`:

```typescript
/**
 * @typedef {{
 *   order: !SortOrder,
 *   column: !ColumnName,
 *   status: !PropStatus,
 * }}
 */
export let Sorting;

/**
 * Types of the view for the right-hand side of the Files app.
 * @enum {string }
 */
export const ViewType = {
  LIST: 'LIST-VIEW',
  GRID: 'GRID-VIEW',
}

/**
 * @typedef {{
 *   view: !ViewType,
 *   status: !PropStatus,
 * }}
 */
export let DetailView;

/**
 * User's UI Settings.
 * @typedef {{
 *   sort: !Sorting,
 *   detailView: !DetailView,
 * }}
 */
export let UiSettings;
```

The action and the reducer changes to account for the new status.

```ts
export const changeSortingOrder = slice.addReducer('change-sort-order', (state: State, payload: {
    order: SortOrder,
    column: ColumnName,
    status: PropStatus,
  }) => ({
    ...state,
    uiSettings: {
      ...uiSettings,
      sort: {
        status: payload.status,
        order: payload.order,
        column: payload.column,
      },
    },
  }));

export const switchDetailView = slice.addReducer('switch-detail-view', (state: State: payload: {
    view: ViewType,
    status: PropStatus,
  }) => ({
    ...state,
    uiSettings: {
      ...uiSettings,
      detailView: {
        status: payload.status,
        view: payload.view,
      },
    },
  }));
```

For the Actions Producer, we create a new file:
`./actions_producers/ui_settings.ts`.

```ts
const VIEW_KEY = 'ui-settings-view-key';
const SORTING_KEY = 'ui-settings-sorting-key';

export async function*
    switchDetailViewProducer(): ActionsProducerGen {
  const state = getStore().getState();
  const viewType = state.uiSettings.detailView.view === ViewType.LIST ?
      ViewType.GRID :
      ViewType.LIST;

  yield switchDetailView(viewType, PropStatus.STARTED);

  await storage.local.setAsync({[VIEW_KEY]: viewType});

  yield switchDetailView(viewType, PropStatus.SUCCESS);
}

export const switchDetailView = serialize(switchDetailViewProducer);

let changeSortingOrderProducer = async function*(order: SortOrder, column: ColumnName):
        ActionsProducerGen {
  yield changeSortingOrder(order, column, PropStatus.STARTED);

  await storage.local.setAsync({[SORTING_KEY]: {order, column}});

  yield changeSortingOrder(order, column, PropStatus.SUCCESS);
}

changeSortingOrderProducer = keepLatest(changeSortingOrderProducer);

export changeSortingOrderProducer;
```

In the container side, we dispatch the Actions Producer in the same way we
dispatch Actions.

We only use the AP wrapped by the concurrency model.

```ts
class ToolbarContainer {
  onSwitchButtonClick_(event) {
    this.store_.dispatch(switchDetailView());
  }

  onSortButtonClick_(event) {
    // Figure out the sorting order from the event.
    const sortOrder = event.target.dataset.order === 'descending' ?
        SortOrder.DESC :
        SortOrder.ASC;

    this.store_.dispatch(changeSortingOrder(order: sortOrder));
  }
}
```
## Add a new selector

Continuing with our previous example, let's add a selector by combining the
default selector provided to us by the slice we created.

The new selector indicates that either `sort.order` or `detailView.view` has
changed:

```ts
const sortingOrder = combine1Selector(
      (uiSettings) => uiSettings.sort.order, uiSettingsSlice.selector);

const detailViewType = combine1Selector(
      (uiSettings) => uiSettings.detailView.view, uiSettingsSlice.selector);

// Will always emit a new object, but will only be triggered when either one of
// its parents (sortingOrder, detailViewType) have emitted a new value.
const sortingFinished = combine2Selectors(
      (order, detailView) => ({order, detailView}), sortingOrder, detailViewType);
```
