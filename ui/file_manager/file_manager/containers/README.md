# Containers

Containers are used for classes that will control multiple widgets. These
classes should:

1.  Listen to custom events from the widgets and convert them to Files app
    specific feature, usually by dispatching an Action to the Store.
2.  Get state from the Store and pass it to the widget by properties or
    attributes.
3.  Coordinate between different children widgets, e.g. listen to custom event
    from one widget (child element 1), and pass the event data to another widget
    (child element 2) via properties or attributes.

## Example

```html
<xf-widget1 attr1="aaa"></xf-widget1>
<xf-widget2 attr2="bbb"></xf-widget2>
```

### Listen to custom events and dispatch actions

```typescript
connectedCallback() {
  const widget1 = this.shadowRoot.querySelector('xf-widget1');
  widget1.addEventListener('widget1-clicked', (e) => {
    this.store_.dispatch(filesSpecificAction({data: e.detail.data}));
  });
}
```

### Get state from the Store and pass it to the widget

```typescript
onStateChanged(state: State) {
  const {customKey} = state;
  const widget1 = this.shadowRoot.querySelector('xf-widget1');
  widget1.setAttribute('attr1', customKey);
}
```

### Coordinate between children widgets

```typescript
connectedCallback(state: State) {
  const widget1 = this.shadowRoot.querySelector('xf-widget1');
  const widget2 = this.shadowRoot.querySelector('xf-widget2');
  widget1.addEventListener('widget1-clicked', (e) => {
    widget2.setAttribute('attr2', e.detail.data);
  });
}
```

## Deps

Code in this folder can depend on other folders such as `../widgets`,
`../state/`, `../lib/`, etc.
