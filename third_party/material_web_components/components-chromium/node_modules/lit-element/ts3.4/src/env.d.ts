interface ShadyCSS {
    styleElement(host: Element, overrideProps?: {
        [key: string]: string;
    }): void;
    getComputedStyleValue(element: Element, property: string): string;
    ScopingShim: undefined | {
        prepareAdoptedCssText(cssText: string[], name: string): void;
    };
    nativeShadow: boolean;
}
interface ShadyDOM {
    inUse: boolean;
    flush: () => void;
}
interface Window {
    ShadyCSS?: ShadyCSS;
    ShadyDOM?: ShadyDOM;
    ShadowRoot: typeof ShadowRoot;
}
// Augment existing types with styling API
interface ShadowRoot {
    adoptedStyleSheets: CSSStyleSheet[];
}
declare var ShadowRoot: {
    prototype: ShadowRoot;
    new (): ShadowRoot;
};
interface CSSStyleSheet {
    replaceSync(cssText: string): void;
    replace(cssText: string): Promise<unknown>;
}
