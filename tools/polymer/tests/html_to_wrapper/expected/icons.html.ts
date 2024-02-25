import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import {html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const template = html`<iron-iconset-svg name="dummy" size="24">
  <svg>
    <defs>
      <g id="foo"><path d="M12 2C6.48 2 2 6.48 2"></path></g>
    </defs>
  </svg>
</iron-iconset-svg>
`;
document.head.appendChild(template.content);
