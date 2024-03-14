/**
 * When Typescript tries to resolve imports, it sees
 * `import "chrome://resources/mwc/lit/index.js"`. The import path is
 * meaningless at compile time. This file provides Lit's types to builds that
 * that depend on it
 */

export * from "./components-chromium/node_modules/lit/index.js";
export * from "./components-chromium/node_modules/lit/decorators.js";
export * from './components-chromium/node_modules/lit/directive.js';
export * from "./components-chromium/node_modules/lit/directives/async-append.js";
export * from "./components-chromium/node_modules/lit/directives/async-replace.js";
export * from "./components-chromium/node_modules/lit/directives/cache.js";
export * from "./components-chromium/node_modules/lit/directives/choose.js";
export * from "./components-chromium/node_modules/lit/directives/class-map.js";
export * from "./components-chromium/node_modules/lit/directives/guard.js";
export * from "./components-chromium/node_modules/lit/directives/if-defined.js";
export * from "./components-chromium/node_modules/lit/directives/join.js";
export * from "./components-chromium/node_modules/lit/directives/keyed.js";
export * from "./components-chromium/node_modules/lit/directives/live.js";
export * from "./components-chromium/node_modules/lit/directives/map.js";
export * from "./components-chromium/node_modules/lit/directives/range.js";
export * from "./components-chromium/node_modules/lit/directives/ref.js";
export * from "./components-chromium/node_modules/lit/directives/repeat.js";
export * from "./components-chromium/node_modules/lit/directives/style-map.js";
export * from "./components-chromium/node_modules/lit/directives/template-content.js";
export * from "./components-chromium/node_modules/lit/directives/unsafe-html.js";
export * from "./components-chromium/node_modules/lit/directives/unsafe-svg.js";
export * from "./components-chromium/node_modules/lit/directives/until.js";
export * from './components-chromium/node_modules/lit/directives/when.js';
export * from './components-chromium/node_modules/@lit/task/task.js';
export {html as staticHtml, literal, svg as staticSvg, unsafeStatic, withStatic,} from './components-chromium/node_modules/lit/static-html.js';
