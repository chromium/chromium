import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
const template = html`
<iron-iconset-svg name="cr_foo_20" size="20">
  <svg>
    <defs>
      <g id="add"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></g>
    </defs>
  </svg>
</iron-iconset-svg>
<iron-iconset-svg name="cr_foo_24" size="24">
  <svg>
    <defs>
      <g id="add"><path d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"/></g>
    </defs>
  </svg>
</iron-iconset-svg>
`;
document.head.appendChild(template.content);
