'use strict';

// This is a direct copy from here:
// third_party/blink/web_tests/external/wpt/html/resources/common.js
// These constants should be kept in sync.

const HTML5_ELEMENTS = [
  'a',        'abbr',     'address',  'area',     'article',    'aside',
  'audio',    'b',        'base',     'bdi',      'bdo',        'blockquote',
  'body',     'br',       'button',   'canvas',   'caption',    'cite',
  'code',     'col',      'colgroup', 'data',     'datalist',   'dd',
  'del',      'details',  'dfn',      'dialog',   'div',        'dl',
  'dt',       'em',       'embed',    'fieldset', 'figcaption', 'figure',
  'footer',   'form',     'h1',       'h2',       'h3',         'h4',
  'h5',       'h6',       'head',     'header',   'hr',         'html',
  'i',        'iframe',   'img',      'input',    'ins',        'kbd',
  'label',    'legend',   'li',       'link',     'main',       'map',
  'mark',     'menu',     'meta',     'meter',    'nav',        'noscript',
  'object',   'ol',       'optgroup', 'option',   'output',     'p',
  'param',    'pre',      'progress', 'q',        'rp',         'rt',
  'ruby',     's',        'samp',     'script',   'section',    'select',
  'slot',     'small',    'source',   'span',     'strong',     'style',
  'sub',      'sup',      'summary',  'table',    'tbody',      'td',
  'template', 'textarea', 'tfoot',    'th',       'thead',      'time',
  'title',    'tr',       'track',    'u',        'ul',         'var',
  'video',    'wbr'
];

// only void (without end tag) HTML5 elements
var HTML5_VOID_ELEMENTS = [
  'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta',
  'param', 'source', 'track', 'wbr'
];

// https://html.spec.whatwg.org/multipage/multipage/forms.html#form-associated-element
var HTML5_FORM_ASSOCIATED_ELEMENTS =
    ['button', 'fieldset', 'input', 'object', 'output', 'select', 'textarea'];

const HTML5_SHADOW_ALLOWED_ELEMENTS = [
  'article', 'aside', 'blockquote', 'body', 'div', 'footer', 'h1', 'h2', 'h3',
  'h4', 'h5', 'h6', 'header', 'main', 'nav', 'p', 'section', 'span'
];

const HTML5_SHADOW_DISALLOWED_ELEMENTS = HTML5_ELEMENTS.filter(
    el => !HTML5_SHADOW_ALLOWED_ELEMENTS.includes(el));

// These are *deprecated/removed* HTML5 element names.
const HTML5_DEPRECATED_ELEMENTS = [
  'acronym',  'applet',  'basefont', 'bgsound',  'big',       'blink',
  'center',   'command', 'content',  'dir',      'font',      'frame',
  'frameset', 'hgroup',  'image',    'isindex',  'keygen',    'marquee',
  'menuitem', 'nobr',    'noembed',  'noframes', 'plaintext', 'rb',
  'rtc',      'shadow',  'spacer',   'strike',   'tt',        'xmp'
];
