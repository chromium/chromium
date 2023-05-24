/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Library that provides safe getters for different node properties and
 * checks for clobbering.
 */
/** Gets a reasonable nodeName, even for clobbered nodes. */
export declare function getNodeName(node: Node): string;
/** Returns true if the object passed is a Text node. */
export declare function isText(node: Node): node is Text;
/** Returns true if the object passed is an Element node. */
export declare function isElement(node: Node): node is Element;
