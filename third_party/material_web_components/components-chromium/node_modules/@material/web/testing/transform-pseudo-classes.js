/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Array of pseudo classes to transform by default. These pseudo classes
 * represent state interactions from the user (such as :hover) or the browser
 * (such as :autofill) that cannot be reproduced with HTML markup.
 */
export const defaultTransformPseudoClasses = [
    ':active',
    ':autofill',
    ':focus',
    ':focus-visible',
    ':focus-within',
    ':hover',
    ':invalid',
    ':link',
    ':paused',
    ':playing',
    ':user-invalid',
    ':valid',
    ':visited',
];
/**
 * Retrieves the transformed class name for a given pseudo class.
 *
 * @param pseudoClass The pseudo class to transform.
 * @return The transform pseudo class string.
 */
export function getTransformedPseudoClass(pseudoClass) {
    return `_${pseudoClass.substring(1)}`;
}
/**
 * A weak set of stylesheets to use as reference for whether or not a stylesheet
 * has been transformed.
 */
const transformedStyleSheets = new WeakSet();
/**
 * Transforms a document's stylesheets' pseudo classes into normal classes with
 * a new stylesheet.
 *
 * Pseudo classes are given an underscore in their transformation. For example,
 * `:hover` transforms to `._hover`.
 *
 * ```css
 * .mdc-foo:hover {
 *   color: teal;
 * }
 * ```
 * ```css
 * .mdc-foo._hover {
 *   color: teal;
 * }
 * ```
 *
 * @param pseudoClasses An optional array of pseudo class names to transform.
 */
export function transformPseudoClasses(stylesheets, pseudoClasses = defaultTransformPseudoClasses) {
    for (const stylesheet of stylesheets) {
        if (transformedStyleSheets.has(stylesheet)) {
            continue;
        }
        let rules;
        try {
            rules = stylesheet.cssRules;
        }
        catch {
            continue;
        }
        for (let j = rules.length - 1; j >= 0; j--) {
            visitRule(rules[j], stylesheet, j, pseudoClasses);
        }
        transformedStyleSheets.add(stylesheet);
    }
}
/**
 * Determines whether or not the CSSRule is a CSSGroupingRule.
 *
 * Cannot check instanceof because FF treats a CSSStyleRule as a subclass of
 * CSSGroupingRule unlike Chrome and Safari
 */
function isCSSGroupingRule(rule) {
    return (!!rule?.cssRules &&
        !rule.selectorText);
}
/**
 * Visits a rule for the given stylesheet and adds a rule that replaces any
 * pseudo classes with a regular transformed class for simulation styling.
 *
 * @param rule The CSS rule to transform.
 * @param stylesheet The rule's parent stylesheet to update.
 * @param index The index of the rule in the parent stylesheet.
 * @param pseudoClasses An array of pseudo classes to search for and replace.
 */
function visitRule(rule, stylesheet, index, pseudoClasses) {
    if (isCSSGroupingRule(rule)) {
        for (let i = rule.cssRules.length - 1; i >= 0; i--) {
            visitRule(rule.cssRules[i], rule, i, pseudoClasses);
        }
        return;
    }
    if (!(rule instanceof CSSStyleRule)) {
        return;
    }
    try {
        let { selectorText } = rule;
        // match :foo, ensuring that it does not have a paren at the end
        // (no pseudo class functions like :foo())
        const regex = /(:(?![\w-]+\()[\w-]+)/g;
        const matches = Array.from(selectorText.matchAll(regex)).filter((match) => {
            // don't match pseudo elements like ::foo
            if (match.index != null && selectorText[match.index - 1] === ':') {
                return false;
            }
            return pseudoClasses.includes(match[1]);
        });
        if (!matches.length) {
            return;
        }
        matches.reverse();
        selectorText = rearrangePseudoElements(selectorText);
        for (const match of matches) {
            selectorText =
                selectorText.substring(0, match.index) +
                    `.${getTransformedPseudoClass(match[1])}` +
                    selectorText.substring(match.index + match[1].length);
        }
        const css = `${selectorText} {${rule.style.cssText}}`;
        stylesheet.insertRule(css, index + 1);
    }
    catch (error) {
        // Catch exception to skip the rule that cannot be parsed.
        console.error(error);
    }
}
/**
 * Re-arranges a selector's pseudo elements to appear at the end of the
 * selector. This prevents invalid CSS when replacing pseudo classes that
 * appear after a pseudo element.
 *
 * @example
 * // '.foo::before:hover' -> '.foo::before._hover' is invalid
 *
 * rearrangePseudoElements('.foo::before:hover'); // '.foo:hover::before'
 * // '.foo:hover::before' -> '.foo._hover::before' is valid
 *
 * @param selectorText The selector text string to re-arrange.
 * @return The re-arranged selector text.
 */
function rearrangePseudoElements(selectorText) {
    const pseudoElementsBeforeClasses = Array.from(selectorText.matchAll(/(?:::[\w-]+)+(?=:[\w-])/g));
    pseudoElementsBeforeClasses.reverse();
    for (const match of pseudoElementsBeforeClasses) {
        const pseudoElement = match[0];
        const pseudoElementIndex = match.index;
        const endOfCompoundSelector = selectorText
            .substring(pseudoElementIndex)
            .match(/(\s(?!([^\s].)*\))|,|$)/);
        const index = endOfCompoundSelector.index + pseudoElementIndex;
        selectorText =
            selectorText.substring(0, index) +
                pseudoElement +
                selectorText.substring(index);
        selectorText =
            selectorText.substring(0, pseudoElementIndex) +
                selectorText.substring(pseudoElementIndex + pseudoElement.length);
    }
    return selectorText;
}
//# sourceMappingURL=transform-pseudo-classes.js.map