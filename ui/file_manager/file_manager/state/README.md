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
  payload: {
    newDirectory: '-- URL to the directory --',
  },
}
```

## Actions Producer

Actions Producers (AP) are async generator functions, that call asynchronous
APIs and yields Actions to the Store.

Each AP should be wrapped in a concurrency model to guarantee that the execution
is still valid in-between each async API call.

See more details in the internal design doc:
http://go/files-app-store-async-concurrency

## Reducers

Reducers are functions that take the current state and an `Action` and performs
the actual change to the state and returns the new state.

## State

The interface `State` in `../externs/ts/state.js` describe the shape and types
of the Files app state.

At runtime, the state is just a plain Object the interface is used throughout
the code to guarantee that all the parts are managing the plain Object with
correct types.

## Adding new State

Every part of the State should be typed. While we still support Closure type
check we maintain the type definition using `@typedef` in
`../externs/ts/state.js` which is understood by both Closure and TS compilers.

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

## Adding new Actions

Every new action requires the following things:

1. A new entry in the enum `ActionType`.
1. A new interface describing the shape of the action's payload, this must
   inherit from BaseAction.
1. Append the new interface to the union type `Action`.
1. A Reducer to apply the action to the store/state.

Let's continue the example of the UI Settings and create an action to update the
`uiSettings` state.

For this example, we'll create 2 actions because we want to
update `sortOrder` independently of `detailView`.

For simplicity, this data will only live in memory, therefore we can update the
Store synchronously. See the section [Add new Actions
Producer](#add-new-actions-producer) for an example of sending this to a storage
backend asynchronously.

In `./actions.ts`.

```typescript
export type Action = ...|ChangeSortOrderAction|SwitchDetailViewAction;

export const enum ActionType {
  ...
  CHANGE_SORT_ORDER = 'change-sort-order',
  SWITCH_DETAIL_VIEW = 'switch-detail-view',
}

export interface ChangeSortOrderAction extends BaseAction {
  type: ActionType.CHANGE_SORT_ORDER;
  payload: {
    order: SortOrder,
    column: ColumnName,
  };
}

export interface SwitchDetailViewAction extends BaseAction {
  type: ActionType.SWITCH_DETAIL_VIEW;
  // No payload, this switches from one to another.
}

/** Returns an action to update the sortOrder UI settings. */
export function changeSortOrder(
    order: SortOrder, column: ColumnName): ChangeSortOrderAction {
  return {
    type: ActionType.CHANGE_SORT_ORDER,
    payload: {
      order,
      column,
    },
  };
}

/** Returns an action to update the sortOrder UI settings. */
export function switchDetailView(): SwitchDetailViewAction {
  return { type: ActionType.SWITCH_DETAIL_VIEW };
}
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

Add a new reducer: `./reducers/ui_settings.ts`.

```typescript
export function updateUiSettings(
    state: State,
    action: ChangeSortOrderAction|SwitchDetailViewAction): UiSettings {
  const uiSettings = state.uiSettings;
  if (action.type === ActionType.CHANGE_SORT_ORDER) {
    return {
      ...uiSettings,
      sort: {
        order: action.payload.order,
        column: action.payload.column,
      },
    };
  }

  if (action.type === ActionType.SWITCH_DETAIL_VIEW) {
    return {
      ...uiSettings,
      detailView: uiSettings.detailView === ViewType.LIST ? ViewType.GRID :
                                                            ViewType.LIST,
    };
  }

  return uiSettings;
}
```

Register the new reducer `ui_settings.ts` in the `../file_names.gni`.

```python
ts_files = [
   ...
  "file_manager/state/reducers/ui_settings.ts",
]
```

Register the new reducer in the rootReducer() `./reducers/root.ts`.

```typescript
import {updateUiSettings} from './ui_settings.js';

export function rootReducer(currentState: State, action: Action): State {
  ...
  switch (action.type) {
    ...
    case ActionType.CHANGE_SORT_ORDER:
    case ActionType.SWITCH_DETAIL_VIEW:
      return Object.assign(state, {
        uiSettings: updateUiSettings(state, action),
      });
  }
}
```

Use the new actions in a container.

For example a hypothetical "toolbar_container.ts" handling the "switch-view"
button and the "sort-button".

```typescript
import {switchDetailView} from '../state/actions.js';

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

See a working CL with this code in http://crrev.com/c/3895339.

In file `../externs/ts/state.js`:

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

```typescript
// We replaced the `SWITCH_DETAIL_VIEW` (from section "Adding new Actions") with
// a `CHANGE_DETAIL_VIEW`.
export type Action = ...|ChangeDetailViewAction|
export const enum ActionType {
  ...
  CHANGE_DETAIL_VIEW = 'change-detail-view',
}

export interface ChangeDetailViewAction extends BaseAction {
  type: ActionType.CHANGE_DETAIL_VIEW;
  payload: {
    view: ViewType,
    status: PropStatus,
  };
}

// The action ChangeSortOrderAction just got the `status` in the payload.
export interface ChangeSortOrderAction extends BaseAction {
  type: ActionType.CHANGE_SORT_ORDER;
  payload: {
    order: SortOrder,
    column: ColumnName,
    status: PropStatus,
  };
}
```

The reducer is slightly different, in file: `./reducers/ui_settings.ts`.

```typescript
export function updateUiSettings(
    state: State,
    action: ChangeSortOrderAction|ChangeDetailViewAction): UiSettings {
  const uiSettings = state.uiSettings;
  if (action.type === ActionType.CHANGE_SORT_ORDER) {
    return {
      ...uiSettings,
      sort: {
        status: action.payload.status,
        order: action.payload.order,
        column: action.payload.column,
      },
    };
  }

  if (action.type === ActionType.CHANGE_DETAIL_VIEW) {
    return {
      ...uiSettings,
      detailView: {
        status: action.payload.status,
        view: action.payload.view,
      }
    };
  }

  return uiSettings;
}
```

The new action has to be registered in the rootReducer() `./reducers/root.ts`.

```typescript
export function rootReducer(currentState: State, action: Action): State {

  switch (action.type) {
    ...
    case ActionType.CHANGE_SORT_ORDER:
    case ActionType.CHANGE_DETAIL_VIEW:
      return Object.assign(state, {
        uiSettings: updateUiSettings(state, action),
      });
  }
```

For the Actions Producer, we create a new file:
`./actions_producers/ui_settings.ts`.

```typescript
import {storage} from '../../common/js/storage.js';
import {ColumnName, PropStatus, SortOrder, ViewType} from '../../externs/ts/state.js';
import {ActionsProducerGen} from '../../lib/actions_producer.js';
import {keepLatest, serialize} from '../../lib/concurrency_models.js';
import {ActionType, ChangeDetailViewAction, ChangeSortOrderAction} from '../actions.js';
import {getStore} from '../store.js';

const VIEW_KEY = 'ui-settings-view-key';
const SORTING_KEY = 'ui-settings-sorting-key';

export async function*
    switchDetailViewProducer(): ActionsProducerGen<ChangeDetailViewAction> {
  const state = getStore().getState();
  const viewType = state.uiSettings.detailView.view === ViewType.LIST ?
      ViewType.GRID :
      ViewType.LIST;
  yield {
    type: ActionType.CHANGE_DETAIL_VIEW,
    payload: {
      view: viewType,
      status: PropStatus.STARTED,
    },
  };

  await storage.local.setAsync({[VIEW_KEY]: viewType});

  yield {
    type: ActionType.CHANGE_DETAIL_VIEW,
    payload: {
      view: viewType,
      status: PropStatus.SUCCESS,
    },
  };
}

export const switchDetailView = serialize(switchDetailViewProducer);

export async function*
    changeSortingOrderProducer(order: SortOrder, column: ColumnName):
        ActionsProducerGen<ChangeSortOrderAction> {
  yield {
    type: ActionType.CHANGE_SORT_ORDER,
    payload: {
      order,
      column,
      status: PropStatus.STARTED,
    },
  };

  await storage.local.setAsync({[SORTING_KEY]: {order, column}});

  yield {
    type: ActionType.CHANGE_SORT_ORDER,
    payload: {
      order,
      column,
      status: PropStatus.SUCCESS,
    },
  };
}

export const changeSortingOrder = keepLatest(changeSortingOrderProducer);
```

In the container side, we dispatch the Actions Producer in the same way we
dispatch Actions.

We only use the AP wrapped by the concurrency model.

```typescript
import {changeSortingOrder, switchDetailViewfrom} '../state/actions_producers/ui_settings.js';

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
