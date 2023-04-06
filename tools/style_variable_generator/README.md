# style_variable_generator

This is a python tool that generates cross-platform style variables in order to
centralize UI constants.

This script uses third_party/pyjson5 to read input json5 files and then
generates various output formats as needed by clients (e.g CSS Variables,
preview HTML page).

For input format examples, see the \*_test.json5 files which contain up to date
illustrations of each feature, as well as expected outputs in the corresponding
\*_test_expected.\* files.

Run `./style_variable_generator -h` for usage details.

## Typescript

The ts generator mode will output a typescript file which exports all available
colors as ts constants. This allows users to directly import colors to use in
lit components.

```ts
import {html, css, LitElement} from 'lit';
import {customElement, property} from 'lit/decorators.js';
// colors.ts is the output of the script.
import {TEXT_COLOR_PRIMARY} from 'colors.ts';

@customElement('simple-greeting')
export class SimpleGreeting extends LitElement {
  static styles = css`p { color: ${TEXT_COLOR_PRIMARY}} }`;

  @property()
  name = 'Somebody';

  render() {
    return html`<p>Hello, ${this.name}!</p>`;
  }
}
```

The generated ts file has a single dependency to `lit-element` which your
project will need to be able to resolve when it compiles against it.

**NOTE:** This file does not export all the colors as rgb strings, but rather
each exported constant just points to a css variable. Before you can use these
constants you need to include the css variables in your app. The recommended way
in chromium is to add a `<link>` in `<head>` which points to
`chrome://resources/chromeos/colors/cros_styles.css`.

If you are using semantic colors in a situation where relying on
`chrome://resources` is infeasible you can specify
`--generator-option 'include_style_sheet=true'` and then call `getColorsCSS`
from the generated ts file. This is primarily intended for projects that live
outside of chromium and need to ship with an isolated bundle of colors. Once you
have the colors you can then add the returned string to your dom via

```
const allColors = getColorsCSS();
const styleSheet = new CSSStyleSheet();
styleSheet.replaceSync(allColors);
document.adoptedStyleSheets = [styleSheet];
```

Ensure you run this code before attempting to render the rest of the application
so all TS constants correctly resolve.

> Note: If you are in Google3 use the `installColors` helper from
> //third_party/py/chrome_styles instead which handles non chrome browsers and
> security considerations.

## Generator Options

### CSS

**Prefix**

```
{
    options: {
        CSS: {
            prefix: 'very'
        }
    },
    colors: {
        important_color: '#ffffff'
    }
}
```

Puts a prefix before all css variables generated i.e `important_color` will
become `--very-important-color` in the example above.

NOTE: The typescript generator extends the css generator, as such the css that
the typescript file generates will respect the prefix option defined in
options.css.prefix.

**Preblend**

```
{
    options: {
        CSS: {
            preblend: true
        }
    },
    colors: {
        color: 'blend(black, rgba(255, 255, 255, .3))'
    }
}
```

By default the css generator will output blends as `color-mix` calls. However
when preblend is specified as true this is ignored and all blends are preblended
at compile time and their final rgb value is outputted to the css.

**Dark mode selector**

`--generator-option 'dark_mode_selector=html[dark]'`

Replaces the default media query (`@media (prefers-color-scheme: dark)`) which
triggers colors to switch to dark mode with a custom css selector. The example
above would produce

```
html[dark] {
    ...dark mode colors
}
```

instead of the default

```
@media (prefers-color-scheme: dark) {
    html:not(body) {
        ... colors
    }
}
```

This should only be used if you want to generate a stylesheet for testing where
you can control the switch to dark/light mode, in production always prefer to
use the default behavior which will respect operating system level dark mode
switches.

**Suppress Sources Comment**

If true suppresses adding a comment to the generated output file with a list of all the sources used to generate the file.

`--generator-option 'suppress_sources_comment=true'`

Defaults to false.

### TS

The typescript generator _extends_ the CSS generator so additionally supports all the options from the CSS generator.

**Include StyleSheet**

`--generator-option 'include_style_sheet=true'`

If true the generated ts file will also include a function called
`initializeColors` which when called will attach all colors as css variables to
the document root. Useful for cases where you don't want to include the colors
as a css file. In these cases ensure that this is called before any usage of
the ts constants, ideally before the root lit element of an app is rendered.

Defaults to false.

### Proto

**field_name**

```
{
    options: {
        proto: {
            field_name: 'test_colors'
        }
    },
    colors: {
        important_color: '#ffffff'
    }
}
```

Name of the field in the output colors message which will contain all exported
colors.


**field_id**

```
{
    options: {
        proto: {
            field_id: 2
        }
    },
    colors: {
        important_color: '#ffffff'
    }
}
```
Id of the field in the output colors message which will contain all exported
colors.
