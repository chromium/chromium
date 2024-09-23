import {LegacyElementMixin} from '../polymer/lib/legacy/legacy-element-mixin.js';

interface PaperRippleElement extends LegacyElementMixin, HTMLElement {
  center: boolean;
  holdDown: boolean;
  noink: boolean;
  recenters: boolean;

  clear(): void;
  downAction(e?: Event): void;
  showAndHoldDown(): void;
  simulatedRipple(): void;
  uiDownAction(e?: Event): void;
  uiUpAction(e?: Event): void;
  upAction(e?: Event): void;
}

export {PaperRippleElement};

declare global {
  interface HTMLElementTagNameMap {
    'paper-ripple': PaperRippleElement;
  }
}
