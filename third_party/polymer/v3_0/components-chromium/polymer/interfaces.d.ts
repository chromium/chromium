/**
 * This file contains the types that are required for compilation of the
 * Polymer generated type declarations, but which could not themselves be
 * automatically generated.
 */

// Types from "externs/polymer-externs.js"

export interface PolymerElementPropertiesMeta {
  type?: Function;
  value?: any;
  readOnly?: boolean;
  computed?: string;
  reflectToAttribute?: boolean;
  notify?: boolean;
  observer?: string|((val: any, old: any) => void);
}

export type PolymerElementProperties = {
  [key: string]: PolymerElementPropertiesMeta|Function;
};

// TODO Document these properties.
export interface PolymerInit {
  is: string;
  extends?: string;
  properties?: PolymerElementProperties;
  observers?: string[];
  _template?: HTMLTemplateElement|(() => HTMLTemplateElement);
  hostAttributes?: {[key: string]: any};
  listeners?: {[key: string]: string};
  behaviors?: BehaviorInit | BehaviorInit[];

  // Lifecycle methods
  registered?(): void;
  created?(): void;
  attached?(): void;
  detached?(): void;
  ready?(): void;
  attributeChanged?(name: string, old?: string, value?: string): void;

  // Allow any other user-defined properties
  [others: string]: any;
}

export type BehaviorInit = Pick<
  PolymerInit,
  Exclude<keyof PolymerInit, "is" | "extends" | "_template">
>;

/**
 * The object passed to ".*" wildcard obsevers. A record of a change made to an
 * object.
 * @template B The object matching the non-wildcard portion of the path.
 * @template V Additional types that could be set at the path.
 */
export interface PolymerDeepPropertyChange<B, V> {
  /** Path to the property that changed. */
  path: string;
  /** The object matching the non-wildcard portion of the path. */
  base: B;
  /** New value of the path that changed. */
  value: B|V;
}

/**
 * A record of changes made to an array.
 * @template T The type of the array being observed.
 */
export interface PolymerSplice<T extends Array<{}|null|undefined>> {
  /** Position where the splice started. */
  index: number;
  /** Array of removed items. */
  removed: T;
  /** Number of new items inserted at index. */
  addedCount: number;
  /** A reference to the array in question. */
  object: T;
  /** The string literal 'splice'. */
  type: 'splice';
}

/**
 * The object passed to ".splices" observers. A set of mutations made to the
 * array.
 * @template T The type of the array being observed.
 */
export interface PolymerSpliceChange<T extends Array<{}|null|undefined>> {
  indexSplices: Array<PolymerSplice<T>>;
}

// Types from "externs/polymer-internal-shared-types.js"

export interface StampedTemplate extends DocumentFragment {
  __noInsertionPoint: boolean;
  nodeList: Node[];
  $: {[key: string]: Node};
  templateInfo?: TemplateInfo;
}

export interface NodeInfo {
  id: string;
  events: {name: string, value: string}[];
  hasInsertionPoint: boolean;
  templateInfo: TemplateInfo;
  parentInfo: NodeInfo;
  parentIndex: number;
  infoIndex: number;
  bindings: Binding[];
}

export interface TemplateInfo {
  nodeInfoList: NodeInfo[];
  nodeList: Node[];
  stripWhitespace: boolean;
  hasInsertionPoint?: boolean;
  hostProps: Object;
  propertyEffects: Object;
  childNodes: Node[];
  wasPreBound: boolean;
}

export interface LiteralBindingPart {
  literal: string;
  compoundIndex?: number;
}

export interface MethodArg {
  literal: boolean;
  name: string;
  value: string|number;
  rootProperty?: string;
  structured?: boolean;
  wildcard?: boolean;
}

export interface MethodSignature {
  methodName: string;
  static: boolean;
  args: MethodArg[];
  dynamicFn?: boolean;
}

export interface ExpressionBindingPart {
  mode: string;
  negate: boolean;
  source: string;
  dependencies: Array<MethodArg|string>;
  customEvent: boolean;
  signature: Object|null;
  event: string;
}

export type BindingPart = LiteralBindingPart|ExpressionBindingPart;

export interface Binding {
  kind: string;
  target: string;
  parts: BindingPart[];
  literal?: string;
  isCompound: boolean;
  listenerEvent?: string;
  listenerNegate?: boolean;
}

export interface AsyncInterface {
  run: (fn: Function, delay?: number) => number;
  cancel: (handle: number) => void;
}

// Types from "lib/utils/gestures.html"

export interface GestureRecognizer {
  reset: () => void;
  mousedown?: (e: MouseEvent) => void;
  mousemove?: (e: MouseEvent) => void;
  mouseup?: (e: MouseEvent) => void;
  touchstart?: (e: TouchEvent) => void;
  touchmove?: (e: TouchEvent) => void;
  touchend?: (e: TouchEvent) => void;
  click?: (e: MouseEvent) => void;
}

/**
 * Not defined in the TypeScript DOM library.
 * See https://developer.mozilla.org/en-US/docs/Web/API/IdleDeadline
 */
export interface IdleDeadline {
  didTimeout: boolean;
  timeRemaining(): number;
}

export interface PolymerElementConstructor {
  new (): HTMLElement;
  is?: string;
  extends?: string;
  properties?: PolymerElementProperties;
  observers?: string[];
  template?: string|HTMLTemplateElement|null;
}
