# Files app Widgets

## Overview

Widgets are
[Web Components](https://developer.mozilla.org/en-US/docs/Web/Web_Components)
that have the following responsibility:

1.  Manage user input and events (Keyboard, Mouse, Touch, etc).
2.  Layout and style, the look & feel.
3.  Accessibility (a11y).
4.  Translation/Internationalization (i18n) & Localization (l10n), LTR & RTL
    (Left To Right & Right To Left).

Widgets should NOT:

1.  Handle business logic. Instead it should report the user actions or change
    of local state by emitting events.
2.  Call any private API or storage API.
3.  Use Polymer. Now [LitElement](https://lit.dev/) is preferred.

## How to create a new Widget?

```typescript
import { XfBase, property, customElement, state, css, query } from './xf_base.js';

// Use @customElement decorator to link the component name with the component
// class, no `customElements.define` is required.
@customElement('xf-widget')
// XfBase is our base widget class, all the widgets should extend from it.
export class XfWidget extends XfBase {
    // Section: API contract.

    // Exposed property/attributes.
    // reflect: true is the key to connect attribute with this property.
    @property({type: String, reflect: true}) label = '';
    // We can specify a custom attribute name (otherwise the default
    // attribute name would be "haschildren").
    @property({type: Boolean, reflect: true, attribute: 'has-children'})
    hasChildren = false;

    // Emitted custom events.
    // All the events emitted by this widget should be defined here. Since it's
    // static, the consumer of the widget can also reference the event name by
    // `XfWidget.events.BUTTON_CLICKED`.
    static get events() {
      return {
        BUTTON_CLICKED: 'button_clicked',
      } as const;
    }

    // Exposed public methods.
    children() {
      // Return the slotted element.
      return this.items;
    }

    // Section: Styles.
    static get styles() {
      return getCSS();
    }

    // Section: Internal states.
    // Define internal variables here which will only be used inside the
    // render() (e.g. referred in the template).
    @state() private buttonClicked_ = false;

    // Section: Internal variables.
    // Define internal variables here which won't affect render()
    private boundOnWindowScrolled_ = this.onWindowScrolled_.bind(this);

    // Section: Child DOM elements.
    // We can query the child elements defined in the render() here.
    @query('button') $button!: HTMLButtonElement;
    // We can even query the slotted elements.
    @queryAssignedElements() items!: Array<HTMLSpanElement>;

    // Section: Constructor.
    constructor() {
      // Usually not needed, because the template defined in the render()
      // will be attached to shadow DOM automatically.
    }

    // Section: Render method.
    override render() {
      // * We can pass property/state or any other valid variables to the
      //   template here.
      // * We can event bind event directly on the element, no bind(this) is
      //   required, for example, the <button> click below.
      // * We can do conditional render like this with nested html tag, for
      //   example, only render "Clicked" when the state is true.
      return html`
        <span>${this.label}</span>
        <slot></slot>
        <button @click=${this.onButtonClicked_}>Update label</button>
        ${ this.buttonClicked_ ? html`<p>Clicked</p>` : '' }
      `;
    }

    // Section: Lifecycle methods.
    connectedCallback() {
      // Widget is being added to the document, add event listener for
      // global events.
      document.addEventListener('scroll', this.boundOnWindowScrolled_);
    }

    disconnectedCallback() {
      // Widget is being removed from the document, remove event listener
      // for global events to prevent memory leak.
      document.removeEventListener('scroll', this.boundOnWindowScrolled_);
    }

    // Section: Internal event handlers.
    private onButtonClicked_(e: MouseEvent) {
      this.label = 'something else';
      this.buttonClicked_ = true;
      // Once the property/state has been changed, no need to call render()
      // manually, it will be invoked automatically.

      // Dispatch custom event to the consumer.
      // IMPORTANT: use `bubbles: true, composed: true` to be able to traverse
      // across all Shadow DOMs.
      this.dispatchEvent(new CustomEvent(XfWidget.events.BUTTON_CLICKED, {
        bubbles: true,
        composed: true,
        detail: { label: this.label },
      }));
    }

    private onWindowScrolled_(e: Event) {
      // logic to respond window scroll
    }

    // Section: Other private helper methods.
    private myHelper_() {
      // random helper function
    }
}

// We are define all CSS outside the class so it's easier for
// Developer/Reviewer to focus on other logic in the class.
function getCSS() {
  return css`
    button {
      padding: 2px;
    }
  `;
}

// Section: Type definitions.
// Export the type for the event so caller code can use it in their listener.
export type WidgetButtonClickedEvent = CustomEvent<{label: string}>;
// TS way to tell the compiler about the types of our custom element.
declare global {
  // When dispatching `button_clicked` event that's the type of the event, aka:
  // event.detail.label exists and is a string.
  interface HTMLElementEventMap {
    [XfWidget.events.BUTTON_CLICKED]: WidgetButtonClickedEvent;
  }

  // When fetching the element from the DOM via:
  // document.querySelector('xf-widget');
  // this is the type returned (or null).
  interface HTMLElementTagNameMap {
    'xf-widget': XfWidget;
  }
}
```

## LitElement Features

A lot of features from Native Web Component are also available in the context of
LitElement.

### Slots

[Slots](https://developer.mozilla.org/en-US/docs/Web/Web_Components/Using_templates_and_slots)
are used to populate parts of the template with content managed outside of the
widget.

For example, with the above example `xf-widget` we can pass additional element
as children to it.

```html
<xf-widget>
  <span>child label</span>
</xf-widget>
```

Here the `<span>child label</span>` can replace the `<slot></slot>` in the above
render() function.

Slots can also be used to named parts:

```typescript
// render() function for `example-dialog` widget.
render() {
  return html`
    <h1><slot name="title">Dialog</slot></h1>
    <p><slot name="message"></slot></p>

    <div>
      <slot name="buttons">
        <button id="ok">Ok</button>
      </slot>
    </div>
  `;
}
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

The event listeners should be added by `@<event>` binding in the render()
function to bind event handler to the corresponding event, which is illustrated
in the above code snippet. Alternatively, we can also bind the event in the
`connectedCallback()` and removed it in the `disconnectedCallback()`, see
section [Lifecycle Methods](#lifecycle-methods).

For TypeScript we should declare the types used in the emitted events, which is
illustrated in the above code snippet.

### Expose style

A widget can allow customization of its style using the following features:

*   [CSS variables](https://developer.mozilla.org/en-US/docs/Web/CSS/Using_CSS_custom_properties)
*   [part & ::part()](https://developer.mozilla.org/en-US/docs/Web/CSS/::part)
    and
    [exportparts](https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/exportparts)

```typescript
render() {
  return html`
    <div part="tab active">Tab 1</div>
    <div part="tab">Tab 2</div>
    <div part="tab">Tab 3</div>
  `;
}

function getCSS() {
  return css`
    :host {
      color: var(--my-element-color, #f4f4f4);
      border-radius: var(--my-element-border-radius, 2px);
    }
  `;
}
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

*   `connectedCallback()`: Invoked each time the custom element is appended into
    a document-connected element.
*   `disconnectedCallback()`: Invoked each time the custom element is
    disconnected from the document's DOM.
*   `attributeChangedCallback()`: Invoked each time one of the custom element's
    attributes is added, removed, or changed. Which attributes to notice change
    for is specified in a `static get observedAttributes()` method. With
    LitElement we barely need this callback.
*   `adoptedCallback()`: Invoked each time the custom element is moved to a new
    document.

### A11y

[`tabindex`](https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/tabindex)
and ARIA attributes, especially `aria-label` and `role` are used to enhance the
content for users.

A good start point for a11y is to read ARIA best practices:
https://www.w3.org/WAI/ARIA/apg/

## Tests

Every widget should have its unittest checking the its exposed API. It should
test:

1.  The events the widget emits, e.g.: Show/hide or open/close, content updated,
    user has selected something, etc.
2.  The behavior exposed, parts showing/hiding, the different status it can
    have, etc.
3.  Any internally calculated layout/style/dimensions, e.g.: positioning
    top/left, height/width, etc.

Since the render process is asynchronous and controlled by the Lit library, we
need to explicitly wait for the render to be finished before we can do any
assertion in the unit test. This is usually happening for:

*   the initial render
*   the re-render triggered by property/state change

In LitElement, we can rely on the
[await element.updateComplete](https://lit.dev/docs/components/lifecycle/#updatecomplete)
to explicitly wait for the render to be completed.

```typescript
// For initial render.
document.body.innerHTML = '<xf-widget></xf-widget>';
const element = document.querySelector('xf-widget')!;
element.label = 'abcd';
await element.updateComplete;

// For re-render.
const button = element.shadowRoot!.querySelector('button');
button.click();
await element.updateComplete;
// Assert label change.
```
