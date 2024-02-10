# Polymer to Lit migration steps

## Imports

1) [Automated] Replace PolymerElement import

Replace
  import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
with
  import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

Automation does not handle cases where PolymerElement is imported along with
other stuff in the same import.

2) [Automated] Replace 'getTemplate' import

Replace
  import {getTemplate} from './cr_drawer.html.js';
with
  import {getCss} from './cr_drawer.css.js';
  import {getHtml} from './cr_drawer.html.js';

## Methods

3) [Automated] Replace 'getTemplate()' calls.

Replace
  static get template() {
    return getTemplate();
  }
with
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

4) [Automated] Update 'extends PolymerElement'

Replace "extends PolymerElement" with "extends CrLitElement"

Automation does not cover cases where Mixins are used.

5) [Automated] Update ready() callbacks

Replace
  override ready() {
    super.ready();
    ...
  }
with
  override firstUpdated() {
    ...
  }

6) Convert 'private' methods referred by the HTML template to 'protected'

## Properties

7) [Automated] Update 'properties' getter

Replace
  static get properties() {...}
with
  static override get properties() {...}

8) [Automated] Update 'reflectToAttribute' attribute

Replace 'reflectToAttribute' with 'reflect'

9) [Automated] Update Polymer property shorthand syntax

Replace Polymer shorthand syntax
   heading: String,
with
   heading: {type: String},

10) Comment out observers and add a TODO
Replace
  observer: 'onFooChanged_',
with
  // TODO: Port this observer to Lit
  // observer: 'onFooChanged_',

11) Move value initialization out of 'properties' to the declaration.
Replace
  foo: {
    type: String,
    value: 'foo',
  },
  ...
  value: string;
with
  foo: {
    type: String,
  },
  ...
  value: string = 'foo';

## File structure

12) [Automated] Extract element-specific CSS styles to a dedicated CSS file.
13) [Automated] Remove CSS content from the HTML template.

## Bindings

14) [Automated] Update event listeners syntax in HTML template

Equiavelent vim command
s/on-\([a-zA-Z]\+\)="\([a-zA-Z_]\+\)/@\1="${this.\2}/g

15) Update property access syntax in HTML template

Replace [[...]] with ${this...}
