# Files app Widgets

## Overview

Widgets are [Web
Components](https://developer.mozilla.org/en-US/docs/Web/Web_Components) that
have the following responsibility:

1. Manage user input and events (Keyboard, Mouse, Touch, etc).
2. Layout and style, the look & feel.
3. Accessibility (a11y).
4. Translation/Internationalization (i18n) & Localization (l10n), LTR & RTL
   (Left To Right & Right To Left).

Widgets should NOT:

1. Handle business logic. Instead it should report the user actions or change of
   local state by emitting events.
2. Call any private API or storage API.
3. Use Polymer. It should use native web component, `extends HTMLElement`.

## How to create a new Widget?

Internal documentation: http://go/xf-site/howtos/add-new-ts-file

Example CL: crrev.com/c/3790650

## Native Web Components Features

### Slots

[Slots](https://developer.mozilla.org/en-US/docs/Web/Web_Components/Using_templates_and_slots)
are used to populate parts of the template with content managed outside of the
widget.

For example, for a generic button `<my-button>`, the label of the button should
be managed outside of the button code, `<my-button>The Label</mybutton>`.

```html
<template id="my-button">
  <button class="really-different-button">
    <slot></slot>
  </button>
</template>
```

Slots can be used to named parts:

```html
<template id="example-dialog">
  <h1><slot name="title">Dialog</slot></h1>
  <p> <slot name="message"></slot></p>

  <div>
    <slot name="buttons">
      <button id="ok">Ok</button>
    </slot>
  </div>
</template>
```

The user of the `<example-dialog>` can fill in the slots like:

```html

<example-dialog>
  <span slot="title">Delete?</span>
  <span slot="message">Do you want to delete the file "abc.txt"?</span>
  <div slot="buttons">
    <button id="ok">Yes</button>
    <button id="cancel">No</button>
  </div>
</example-dialog>
```

Content inside the slots can be styled using `::slotted()` pseudo-element:
https://developer.mozilla.org/en-US/docs/Web/CSS/::slotted

### User Input & Events

The widget should convert the user input events like click, keydown, etc to
widget's events, like "item-selected". For example the native `<select>` emits
the
[change](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/change_event)
event whenever the user selects an option either via keyboard or mouse.

The event listeners should be added on the `connectedCallback()` and removed in
the `disconnectedCallback()`, see section [Lifecycle
Methods](#lifecycle-methods).

For TypeScript we should declare the types used in the emitted events.

```typescript
// A const to avoid special strings around the code base.
export const DARK_ROOM_CHANGED = 'dark-room-changed';

// The type of the emitted event. The `event.detail` has an boolean attribute
// `enabled`.
export type DarkRoomChangedEvent = CustomEvent<{enabled: boolean}>;

// TS type mapping.
declare global {
  interface HTMLElementEventMap {
    // Enforces strong typing for listeners of "dark-room-changed" event.
    [DARK_ROOM_CHANGED]: DarkRoomChangedEvent;
  }

  // With this, TS knows that <xf-dark-room> is an instance of `XfDarkRoom`.
  interface HTMLElementTagNameMap {
    'xf-dark-room': XfDarkRoom;
  }
}
```

### Expose style

A widget can allow customization of its style using the following features:

*  [CSS
   variables](https://developer.mozilla.org/en-US/docs/Web/CSS/Using_CSS_custom_properties)
*  [part & ::part()](https://developer.mozilla.org/en-US/docs/Web/CSS/::part)
   and
   [exportparts](https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/exportparts)

```html
<template id="my-element">
  <style>
    :host {
      color: var(--my-element-color, #f4f4f4);
      border-radius: var(--my-element-border-radius, 2px);
    }
  </style>
  <div part="tab active">Tab 1</div>
  <div part="tab">Tab 2</div>
  <div part="tab">Tab 3</div>
</template>
```

```html
<!-- Externally to <my-element> -->
<style>
  :root {
    --my-element-color: red;
    --my-element-border-radius: 4px;
  }
  /* style all the tabs. */
  my-element::part(tab) {
    background-color: cyan;
  }

  /* style the hover tab. */
  my-element::part(tab)::hover {
    background-color: magenta;
  }

  /* style the active/selected tab. */
  my-element::part(active) {
    background-color: blue;
    border: 1px solid grey;
  }
</style>

<my-element></my-element>
```

### Lifecycle Methods

A widget can implement the following methods, see
[MDN](https://developer.mozilla.org/en-US/docs/Web/Web_Components/Using_custom_elements#using_the_lifecycle_callbacks)
for more information.

*  `connectedCallback()`: Invoked each time the custom element is appended into
   a document-connected element.
*  `disconnectedCallback()`: Invoked each time the custom element is
   disconnected from the document's DOM.
*  `attributeChangedCallback()`: Invoked each time one of the custom element's
   attributes is added, removed, or changed. Which attributes to notice change
   for is specified in a `static get observedAttributes()` method.
*  `adoptedCallback()`: Invoked each time the custom element is moved to a new
   document.

### Attributes

Widget can receive data/state via its attributes, for example `<my-element
type="text"></my-element>`.

In this example `type` is the attribute name.
```typescript
class MyElement extends HTMLElement {
  static get observedAttributes() {
    return ['text'];
  }

  attributeChangedCallback(name: string, oldValue: string, newValue: string) {
    if (name === 'type') {
      this.type = newValue;
    }
  }
}
```

### A11y

[`tabindex`](
https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/tabindex)
and ARIA attributes, especially `aria-label` and `role` are used to enhance the
content for users.

A good start point for a11y is to read ARIA best practices:
https://www.w3.org/WAI/ARIA/apg/

## Tests

Every widget should have its unittest checking the its exposed API. It should
test:
1. The events the widget emits, e.g.: Show/hide or open/close, content updated,
   user has selected something, etc.
2. The behavior exposed, parts showing/hiding, the different status it can have,
   etc.
3. Any internally calculated layout/style/dimensions, e.g.: positioning
   top/left, height/width, etc.
