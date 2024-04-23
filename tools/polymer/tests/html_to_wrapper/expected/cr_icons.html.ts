import '//resources/cr_elements/cr_icon/cr_iconset.js';
import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`<cr-iconset name="dummy" size="24">
  <svg>
    <defs>
      <g id="foo"><path d="M12 2C6.48 2 2 6.48 2"></path></g>
    </defs>
  </svg>
</cr-iconset>
`;
const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
