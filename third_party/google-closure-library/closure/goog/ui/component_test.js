/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ComponentTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const DomHelper = goog.require('goog.dom.DomHelper');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const NodeType = goog.require('goog.dom.NodeType');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let component;
const propertyReplacer = new PropertyReplacer();
let sandbox;

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    component = new Component();
  },

  tearDown() {
    component.dispose();
    dom.removeChildren(sandbox);
    propertyReplacer.reset();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testConstructor() {
    assertTrue(
        'Instance must be non-null and have the expected class',
        component instanceof Component);
    assertTrue(
        'DOM helper must be non-null and have the expected class',
        component.dom_ instanceof DomHelper);

    const fakeDom = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const otherComponent = new Component(fakeDom);
    assertEquals(
        'DOM helper must refer to expected object', fakeDom,
        otherComponent.dom_);

    otherComponent.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetId() {
    assertNull('Component ID should be initialized to null', component.id_);
    const id = component.getId();
    assertNotNull('Component ID should be generated on demand', id);
    assertEquals(
        'Subsequent calls to getId() must return same value', id,
        component.getId());
  },

  testSetId() {
    component.setId('myId');
    assertEquals(
        'getId() must return explicitly set ID', 'myId', component.getId());

    const child = new Component();
    const childId = child.getId();
    component.addChild(child);
    assertEquals(
        'Parent component must find child by ID', child,
        component.getChild(childId));

    child.setId('someNewId');
    assertEquals(
        'Parent component must find child by new ID', child,
        component.getChild('someNewId'));

    child.dispose();
  },

  testGetSetElement() {
    assertNull('Element must be null by default', component.getElement());
    const element = dom.createElement(TagName.DIV);
    component.setElementInternal(element);
    assertEquals(
        'getElement() must return expected element', element,
        component.getElement());
  },

  testGetSetParent() {
    assertNull('Parent must be null by default', component.getParent());

    const parent = new Component();
    component.setParent(parent);
    assertEquals(
        'getParent() must return expected component', parent,
        component.getParent());

    component.setParent(null);
    assertNull('Parent must be null', component.getParent());

    assertThrows(
        'Setting a component\'s parent to itself must throw error', () => {
          component.setParent(component);
        });

    parent.addChild(component);
    assertEquals(
        'getParent() must return expected component', parent,
        component.getParent());
    assertThrows(
        'Changing a child component\'s parent must throw error', () => {
          component.setParent(new Component());
        });

    parent.dispose();
  },

  testGetParentEventTarget() {
    assertNull(
        'Parent event target must be null by default',
        component.getParentEventTarget());

    const parent = new Component();
    component.setParent(parent);
    assertEquals(
        'Parent event target must be the parent component', parent,
        component.getParentEventTarget());
    assertThrows(
        'Directly setting the parent event target to other than ' +
            'the parent component when the parent component is set must throw ' +
            'error',
        () => {
          component.setParentEventTarget(new Component());
        });

    parent.dispose();
  },

  testSetParentEventTarget() {
    const parentEventTarget = new GoogEventTarget();
    component.setParentEventTarget(parentEventTarget);
    assertEquals('Parent component must be null', null, component.getParent());

    parentEventTarget.dispose();
  },

  testGetDomHelper() {
    const domHelper = new DomHelper();
    const component = new Component(domHelper);
    assertEquals(
        'Component must return the same DomHelper passed', domHelper,
        component.getDomHelper());
  },

  testIsInDocument() {
    assertFalse(
        'Component must not be in the document by default',
        component.isInDocument());
    component.enterDocument();
    assertTrue('Component must be in the document', component.isInDocument());
  },

  testCreateDom() {
    assertNull(
        'Component must not have DOM by default', component.getElement());
    component.createDom();
    assertEquals(
        'Component\'s DOM must be an element node', NodeType.ELEMENT,
        component.getElement().nodeType);
  },

  testRender() {
    assertFalse(
        'Component must not be in the document by default',
        component.isInDocument());
    assertNull(
        'Component must not have DOM by default', component.getElement());
    assertFalse(
        'wasDecorated() must be false before component is rendered',
        component.wasDecorated());

    component.render(sandbox);
    assertTrue(
        'Rendered component must be in the document', component.isInDocument());
    assertEquals(
        'Component\'s element must be a child of the parent element', sandbox,
        component.getElement().parentNode);
    assertFalse(
        'wasDecorated() must still be false for rendered component',
        component.wasDecorated());

    assertThrows('Trying to re-render component must throw error', () => {
      component.render();
    });
  },

  testRender_NoParent() {
    component.render();
    assertTrue(
        'Rendered component must be in the document', component.isInDocument());
    assertEquals(
        'Component\'s element must be a child of the document body',
        document.body, component.getElement().parentNode);
  },

  testRender_ParentNotInDocument() {
    const parent = new Component();
    component.setParent(parent);

    assertFalse(
        'Parent component must not be in the document', parent.isInDocument());
    assertFalse(
        'Child component must not be in the document',
        component.isInDocument());
    assertNull('Child component must not have DOM', component.getElement());

    component.render();
    assertFalse(
        'Parent component must not be in the document', parent.isInDocument());
    assertFalse(
        'Child component must not be in the document',
        component.isInDocument());
    assertNotNull('Child component must have DOM', component.getElement());

    parent.dispose();
  },

  testRenderBefore() {
    const sibling = dom.createElement(TagName.DIV);
    sandbox.appendChild(sibling);

    component.renderBefore(sibling);
    assertTrue(
        'Rendered component must be in the document', component.isInDocument());
    assertEquals(
        'Component\'s element must be a child of the parent element', sandbox,
        component.getElement().parentNode);
    assertEquals(
        'Component\'s element must have expected nextSibling', sibling,
        component.getElement().nextSibling);
  },

  testRenderChild() {
    const parent = new Component();

    parent.createDom();
    assertFalse('Parent must not be in the document', parent.isInDocument());
    assertNotNull('Parent must have a DOM', parent.getElement());

    parent.addChild(component);
    assertFalse('Child must not be in the document', component.isInDocument());
    assertNull('Child must not have a DOM', component.getElement());

    component.render(parent.getElement());
    assertFalse('Parent must not be in the document', parent.isInDocument());
    assertFalse(
        'Child must not be in the document if the parent isn\'t',
        component.isInDocument());
    assertNotNull('Child must have a DOM', component.getElement());
    assertEquals(
        'Child\'s element must be a child of the parent\'s element',
        parent.getElement(), component.getElement().parentNode);

    parent.render(sandbox);
    assertTrue('Parent must be in the document', parent.isInDocument());
    assertTrue('Child must be in the document', component.isInDocument());

    parent.dispose();
  },

  testDecorate() {
    sandbox.innerHTML = '<div id="foo">Foo</div>';
    const foo = dom.getElement('foo');

    assertFalse(
        'wasDecorated() must be false by default', component.wasDecorated());

    component.decorate(foo);
    assertTrue('Component must be in the document', component.isInDocument());
    assertEquals(
        'Component\'s element must be the decorated element', foo,
        component.getElement());
    assertTrue(
        'wasDecorated() must be true for decorated component',
        component.wasDecorated());

    assertThrows(
        'Trying to decorate with a control already in the document' +
            ' must throw error',
        () => {
          component.decorate(foo);
        });
  },

  testDecorate_AllowDetached_NotInDocument() {
    /** Computed properties to avoid compiler checks of the define value. */
    Component['ALLOW_DETACHED_DECORATION'] = true;
    const element = dom.createElement(TagName.DIV);
    component.decorate(element);
    assertFalse(
        'Component should not call enterDocument when decorated ' +
            'with an element that is not in the document.',
        component.isInDocument());
    /** Computed properties to avoid compiler checks of the define value. */
    Component['ALLOW_DETACHED_DECORATION'] = false;
  },

  testDecorate_AllowDetached_InDocument() {
    /** Computed properties to avoid compiler checks of the define value. */
    Component['ALLOW_DETACHED_DECORATION'] = true;
    const element = dom.createElement(TagName.DIV);
    sandbox.appendChild(element);
    component.decorate(element);
    assertTrue(
        'Component should call enterDocument when decorated ' +
            'with an element that is in the document.',
        component.isInDocument());
    /** Computed properties to avoid compiler checks of the define value. */
    Component['ALLOW_DETACHED_DECORATION'] = false;
  },

  testCannotDecorate() {
    sandbox.innerHTML = '<div id="foo">Foo</div>';
    const foo = dom.getElement('foo');

    // Have canDecorate() return false.
    propertyReplacer.set(component, 'canDecorate', () => false);

    assertThrows(
        'Trying to decorate an element for which canDecorate()' +
            ' returns false must throw error',
        () => {
          component.decorate(foo);
        });
  },

  testCanDecorate() {
    assertTrue(
        'canDecorate() must return true by default',
        component.canDecorate(sandbox));
  },

  testWasDecorated() {
    assertFalse(
        'wasDecorated() must return false by default',
        component.wasDecorated());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDecorateInternal() {
    assertNull('Element must be null by default', component.getElement());
    const element = dom.createElement(TagName.DIV);
    component.decorateInternal(element);
    assertEquals(
        'Element must have expected value', element, component.getElement());
  },

  testGetElementAndGetElementsByClass() {
    sandbox.innerHTML = '<ul id="task-list">' +
        '<li class="task">Unclog drain' +
        '</ul>' +
        '<ul id="completed-tasks">' +
        '<li id="groceries" class="task">Buy groceries' +
        '<li class="task">Rotate tires' +
        '<li class="task">Clean kitchen' +
        '</ul>' +
        assertNull('Should be nothing to return before the component has a DOM',
                   component.getElementByClass('task'));
    assertEquals(
        'Should return an empty list before the component has a DOM', 0,
        component.getElementsByClass('task').length);

    component.decorate(dom.getElement('completed-tasks'));
    assertEquals(
        'getElementByClass() should return the first completed task',
        'groceries', component.getElementByClass('task').id);
    assertEquals(
        'getElementsByClass() should return only the completed tasks', 3,
        component.getElementsByClass('task').length);
  },

  testGetRequiredElementByClass() {
    sandbox.innerHTML = '<ul id="task-list">' +
        '<li class="task">Unclog drain' +
        '</ul>' +
        '<ul id="completed-tasks">' +
        '<li id="groceries" class="task">Buy groceries' +
        '<li class="task">Rotate tires' +
        '<li class="task">Clean kitchen' +
        '</ul>';
    component.decorate(dom.getElement('completed-tasks'));
    assertEquals(
        'getRequiredElementByClass() should return the first completed task',
        'groceries', component.getRequiredElementByClass('task').id);
    assertThrows(
        'Attempting to retrieve a required element that does not' +
            'exist should fail',
        () => {
          component.getRequiredElementByClass('undefinedClass');
        });
  },

  testEnterExitDocument() {
    const c1 = new Component();
    const c2 = new Component();

    component.addChild(c1);
    component.addChild(c2);

    component.createDom();
    c1.createDom();
    c2.createDom();

    assertFalse('Parent must not be in the document', component.isInDocument());
    assertFalse(
        'Neither child must be in the document',
        c1.isInDocument() || c2.isInDocument());

    component.enterDocument();
    assertTrue('Parent must be in the document', component.isInDocument());
    assertTrue(
        'Both children must be in the document',
        c1.isInDocument() && c2.isInDocument());

    component.exitDocument();
    assertFalse('Parent must not be in the document', component.isInDocument());
    assertFalse(
        'Neither child must be in the document',
        c1.isInDocument() || c2.isInDocument());

    c1.dispose();
    c2.dispose();
  },

  testDispose() {
    let c1;
    let c2;

    component.createDom();
    component.addChild((c1 = new Component()), true);
    component.addChild((c2 = new Component()), true);

    const element = component.getElement();
    const c1Element = c1.getElement();
    const c2Element = c2.getElement();

    component.render(sandbox);
    assertTrue('Parent must be in the document', component.isInDocument());
    assertEquals(
        'Parent\'s element must be a child of the sandbox element', sandbox,
        element.parentNode);
    assertTrue(
        'Both children must be in the document',
        c1.isInDocument() && c2.isInDocument());
    assertEquals(
        'First child\'s element must be a child of the parent\'s' +
            ' element',
        element, c1Element.parentNode);
    assertEquals(
        'Second child\'s element must be a child of the parent\'s' +
            ' element',
        element, c2Element.parentNode);

    assertFalse(
        'Parent must not have been disposed of', component.isDisposed());
    assertFalse(
        'Neither child must have been disposed of',
        c1.isDisposed() || c2.isDisposed());

    component.dispose();
    assertTrue('Parent must have been disposed of', component.isDisposed());
    assertFalse('Parent must not be in the document', component.isInDocument());
    assertNotEquals(
        'Parent\'s element must no longer be a child of the' +
            ' sandbox element',
        sandbox, element.parentNode);
    assertTrue(
        'Both children must have been disposed of',
        c1.isDisposed() && c2.isDisposed());
    assertFalse(
        'Neither child must be in the document',
        c1.isInDocument() || c2.isInDocument());
    assertNotEquals(
        'First child\'s element must no longer be a child of' +
            ' the parent\'s element',
        element, c1Element.parentNode);
    assertNotEquals(
        'Second child\'s element must no longer be a child of' +
            ' the parent\'s element',
        element, c2Element.parentNode);
  },

  testDispose_Decorated() {
    sandbox.innerHTML = '<div id="foo">Foo</div>';
    const foo = dom.getElement('foo');

    component.decorate(foo);
    assertTrue('Component must be in the document', component.isInDocument());
    assertFalse(
        'Component must not have been disposed of', component.isDisposed());
    assertEquals(
        'Component\'s element must have expected value', foo,
        component.getElement());
    assertEquals(
        'Decorated element must be a child of the sandbox', sandbox,
        foo.parentNode);

    component.dispose();
    assertFalse(
        'Component must not be in the document', component.isInDocument());
    assertTrue('Component must have been disposed of', component.isDisposed());
    assertNull('Component\'s element must be null', component.getElement());
    assertEquals(
        'Previously decorated element must still be a child of the' +
            ' sandbox',
        sandbox, foo.parentNode);
  },

  testMakeIdAndGetFragmentFromId() {
    assertEquals(
        'Unique id must have expected value', component.getId() + '.foo',
        component.makeId('foo'));
    assertEquals(
        'Fragment must have expected value', 'foo',
        component.getFragmentFromId(component.makeId('foo')));
  },

  testMakeIdsWithObject() {
    const EnumDef = {ENUM_1: 'enum 1', ENUM_2: 'enum 2', ENUM_3: 'enum 3'};
    const ids = component.makeIds(EnumDef);
    assertEquals(component.makeId(EnumDef.ENUM_1), ids.ENUM_1);
    assertEquals(component.makeId(EnumDef.ENUM_2), ids.ENUM_2);
    assertEquals(component.makeId(EnumDef.ENUM_3), ids.ENUM_3);
  },

  testGetElementByFragment() {
    component.render(sandbox);

    /** @suppress {visibility} suppression added to enable type checking */
    const element = component.dom_.createDom(
        TagName.DIV, {id: component.makeId('foo')}, 'Hello');
    sandbox.appendChild(element);

    assertEquals(
        'Element must have expected value', element,
        component.getElementByFragment('foo'));
  },

  testGetSetModel() {
    assertNull('Model must be null by default', component.getModel());

    const model = 'someModel';
    component.setModel(model);
    assertEquals('Model must have expected value', model, component.getModel());

    component.setModel(null);
    assertNull('Model must be null', component.getModel());
  },

  testAddChild() {
    const child = new Component();
    child.setId('child');

    assertFalse('Parent must not be in the document', component.isInDocument());

    component.addChild(child);
    assertTrue('Parent must have children.', component.hasChildren());
    assertEquals(
        'Child must have expected parent', component, child.getParent());
    assertEquals(
        'Parent must find child by ID', child, component.getChild('child'));
  },

  testAddChild_Render() {
    const child = new Component();

    component.render(sandbox);
    assertTrue('Parent must be in the document', component.isInDocument());
    assertEquals(
        'Parent must be in the sandbox', sandbox,
        component.getElement().parentNode);

    component.addChild(child, true);
    assertTrue('Child must be in the document', child.isInDocument());
    assertEquals(
        'Child element must be a child of the parent element',
        component.getElement(), child.getElement().parentNode);
  },

  testAddChild_DomOnly() {
    const child = new Component();

    component.createDom();
    assertNotNull('Parent must have a DOM', component.getElement());
    assertFalse('Parent must not be in the document', component.isInDocument());

    component.addChild(child, true);
    assertNotNull('Child must have a DOM', child.getElement());
    assertEquals(
        'Child element must be a child of the parent element',
        component.getElement(), child.getElement().parentNode);
    assertFalse('Child must not be in the document', child.isInDocument());
  },

  testAddChildAt() {
    const a = new Component();
    const b = new Component();
    const c = new Component();
    const d = new Component();

    a.setId('a');
    b.setId('b');
    c.setId('c');
    d.setId('d');

    component.addChildAt(b, 0);
    assertEquals('b', component.getChildIds().join(''));
    component.addChildAt(d, 1);
    assertEquals('bd', component.getChildIds().join(''));
    component.addChildAt(a, 0);
    assertEquals('abd', component.getChildIds().join(''));
    component.addChildAt(c, 2);
    assertEquals('abcd', component.getChildIds().join(''));

    assertEquals(a, component.getChildAt(0));
    assertEquals(b, component.getChildAt(1));
    assertEquals(c, component.getChildAt(2));
    assertEquals(d, component.getChildAt(3));

    assertThrows('Adding child at out-of-bounds index must throw error', () => {
      component.addChildAt(new Component(), 5);
    });
  },

  testAddChildAtThrowsIfNull() {
    assertThrows('Adding a null child must throw an error', () => {
      component.addChildAt(null, 0);
    });
  },

  testHasChildren() {
    assertFalse('Component must not have children', component.hasChildren());

    component.addChildAt(new Component(), 0);
    assertTrue('Component must have children', component.hasChildren());

    component.removeChildAt(0);
    assertFalse('Component must not have children', component.hasChildren());
  },

  testGetChildCount() {
    assertEquals(
        'Component must have 0 children', 0, component.getChildCount());

    component.addChild(new Component());
    assertEquals('Component must have 1 child', 1, component.getChildCount());

    component.addChild(new Component());
    assertEquals(
        'Component must have 2 children', 2, component.getChildCount());

    component.removeChildAt(1);
    assertEquals('Component must have 1 child', 1, component.getChildCount());

    component.removeChildAt(0);
    assertEquals(
        'Component must have 0 children', 0, component.getChildCount());
  },

  testGetChildIds() {
    const a = new Component();
    const b = new Component();

    a.setId('a');
    b.setId('b');

    component.addChild(a);
    assertEquals('a', component.getChildIds().join(''));

    component.addChild(b);
    assertEquals('ab', component.getChildIds().join(''));

    const ids = component.getChildIds();
    ids.push('c');
    assertEquals(
        'Changes to the array returned by getChildIds() must not' +
            ' affect the component',
        'ab', component.getChildIds().join(''));
  },

  testGetChild() {
    assertNull('Parent must have no children', component.getChild('myId'));

    const c = new Component();
    c.setId('myId');
    component.addChild(c);
    assertEquals('Parent must find child by ID', c, component.getChild('myId'));

    c.setId('newId');
    assertNull(
        'Parent must not find child by old ID', component.getChild('myId'));
    assertEquals(
        'Parent must find child by new ID', c, component.getChild('newId'));
  },

  testGetChildAt() {
    const a = new Component();
    const b = new Component();

    a.setId('a');
    b.setId('b');

    component.addChildAt(a, 0);
    assertEquals('Parent must find child by index', a, component.getChildAt(0));

    component.addChildAt(b, 1);
    assertEquals('Parent must find child by index', b, component.getChildAt(1));

    assertNull(
        'Parent must return null for out-of-bounds index',
        component.getChildAt(3));
  },

  testForEachChild() {
    let invoked = false;
    component.forEachChild((child) => {
      assertNotNull('Child must never be null', child);
      invoked = true;
    });
    assertFalse(
        'forEachChild must not call its argument if the parent has ' +
            'no children',
        invoked);

    component.addChild(new Component());
    component.addChild(new Component());
    component.addChild(new Component());
    let callCount = 0;
    component.forEachChild(function(child, index) {
      assertEquals(component, this);
      callCount++;
    }, component);
    assertEquals(3, callCount);
  },

  testIndexOfChild() {
    const a = new Component();
    const b = new Component();
    const c = new Component();

    a.setId('a');
    b.setId('b');
    c.setId('c');

    component.addChild(a);
    assertEquals(0, component.indexOfChild(a));

    component.addChild(b);
    assertEquals(1, component.indexOfChild(b));

    component.addChild(c);
    assertEquals(2, component.indexOfChild(c));

    assertEquals(
        'indexOfChild must return -1 for nonexistent child', -1,
        component.indexOfChild(new Component()));
  },

  testRemoveChild() {
    const a = new Component();
    const b = new Component();
    const c = new Component();

    a.setId('a');
    b.setId('b');
    c.setId('c');

    component.addChild(a);
    component.addChild(b);
    component.addChild(c);

    assertEquals(
        'Parent must remove and return child', c, component.removeChild(c));
    assertNull(
        'Parent must no longer contain this child', component.getChild('c'));

    assertEquals(
        'Parent must remove and return child by ID', b,
        component.removeChild('b'));
    assertNull(
        'Parent must no longer contain this child', component.getChild('b'));

    assertEquals(
        'Parent must remove and return child by index', a,
        component.removeChildAt(0));
    assertNull(
        'Parent must no longer contain this child', component.getChild('a'));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testMovingChildrenUsingAddChildAt() {
    component.render(sandbox);

    const a = new Component();
    const b = new Component();
    const c = new Component();
    const d = new Component();
    a.setElementInternal(dom.createElement(TagName.A));
    b.setElementInternal(dom.createElement(TagName.B));
    c.setElementInternal(dom.createElement(TagName.C));
    d.setElementInternal(dom.createElement(TagName.D));

    a.setId('a');
    b.setId('b');
    c.setId('c');
    d.setId('d');

    component.addChild(a, true);
    component.addChild(b, true);
    component.addChild(c, true);
    component.addChild(d, true);

    assertEquals('abcd', component.getChildIds().join(''));
    assertEquals(a, component.getChildAt(0));
    assertEquals(b, component.getChildAt(1));
    assertEquals(c, component.getChildAt(2));
    assertEquals(d, component.getChildAt(3));

    // Move child d to the top and b to the bottom.
    component.addChildAt(d, 0);
    component.addChildAt(b, 3);

    assertEquals('dacb', component.getChildIds().join(''));
    assertEquals(d, component.getChildAt(0));
    assertEquals(a, component.getChildAt(1));
    assertEquals(c, component.getChildAt(2));
    assertEquals(b, component.getChildAt(3));

    // Move child a to the top, and check that DOM nodes are in correct order.
    component.addChildAt(a, 0);
    assertEquals('adcb', component.getChildIds().join(''));
    assertEquals(a, component.getChildAt(0));
    assertEquals(a.getElement(), component.getElement().childNodes[0]);

    // Move child d to the bottom, and check that DOM nodes are in correct
    // order. Sanity-check that a re-render occurs.
    const removeChild = recordFunction(Element.prototype.removeChild);
    propertyReplacer.replace(Element.prototype, 'removeChild', removeChild);
    component.addChildAt(d, 3);
    assertEquals('acbd', component.getChildIds().join(''));
    assertEquals(d, component.getChildAt(3));
    assertEquals(d.getElement(), component.getElement().childNodes[3]);
    assertEquals(1, removeChild.getCallCount());

    // Move child d again to the bottom, and check a re-render does not occur
    removeChild.reset();
    component.addChildAt(d, 3);
    assertEquals('acbd', component.getChildIds().join(''));
    assertEquals(d, component.getChildAt(3));
    assertEquals(d.getElement(), component.getElement().childNodes[3]);
    assertEquals(0, removeChild.getCallCount());
  },

  testAddChildAfterDomCreatedDoesNotEnterDocument() {
    const parent = new Component();
    const child = new Component();

    const nestedDiv = dom.createDom(TagName.DIV);
    parent.setElementInternal(dom.createDom(TagName.DIV, undefined, nestedDiv));
    parent.render();

    // Now add a child, whose DOM already exists. This happens, for example,
    // if the child itself performs an addChild(x, true).
    child.createDom();
    parent.addChild(child, false);
    // The parent shouldn't call enterDocument on the child, since the child
    // actually isn't in the document yet.
    assertFalse(child.isInDocument());

    // Now, actually render the child; it should be in the document.
    child.render(nestedDiv);
    assertTrue(child.isInDocument());
    assertEquals(
        'Child should be rendered in the expected div', nestedDiv,
        child.getElement().parentNode);
  },

  testAddChildAfterDomManuallyInserted() {
    const parent = new Component();
    const child = new Component();

    const nestedDiv = dom.createDom(TagName.DIV);
    parent.setElementInternal(dom.createDom(TagName.DIV, undefined, nestedDiv));
    parent.render();

    // This sequence is weird, but some people do it instead of just manually
    // doing render.  The addChild will detect that the child is in the DOM
    // and call enterDocument.
    child.createDom();
    nestedDiv.appendChild(child.getElement());
    parent.addChild(child, false);

    assertTrue(child.isInDocument());
    assertEquals(
        'Child should be rendered in the expected div', nestedDiv,
        child.getElement().parentNode);
  },

  testRemoveChildren() {
    const a = new Component();
    const b = new Component();
    const c = new Component();

    component.addChild(a);
    component.addChild(b);
    component.addChild(c);

    a.setId('a');
    b.setId('b');
    c.setId('c');

    assertArrayEquals(
        'Parent must remove and return children.', [a, b, c],
        component.removeChildren());
    assertNull(
        'Parent must no longer contain this child', component.getChild('a'));
    assertNull(
        'Parent must no longer contain this child', component.getChild('b'));
    assertNull(
        'Parent must no longer contain this child', component.getChild('c'));
  },

  testRemoveChildren_Unrender() {
    const a = new Component();
    const b = new Component();

    component.render(sandbox);
    component.addChild(a);
    component.addChild(b);

    assertArrayEquals(
        'Parent must remove and return children.', [a, b],
        component.removeChildren(true));
    assertNull(
        'Parent must no longer contain this child', component.getChild('a'));
    assertFalse('Child must no longer be in the document.', a.isInDocument());
    assertNull(
        'Parent must no longer contain this child', component.getChild('b'));
    assertFalse('Child must no longer be in the document.', b.isInDocument());
  },

  testSetPointerEventsEnabled() {
    assertFalse(
        'Component must default to mouse events.',
        component.pointerEventsEnabled());

    component.setPointerEventsEnabled(true);
    assertTrue(
        'Component must use pointer events when specified.',
        component.pointerEventsEnabled());

    component.setPointerEventsEnabled(false);
    assertFalse(
        'Component must use mouse events when specified.',
        component.pointerEventsEnabled());
  },

  testSetPointerEventsEnabledAfterEnterDocument() {
    component.render(sandbox);

    assertThrows(
        'setPointerEventsEnabled(true) after enterDocument must throw error.',
        () => {
          component.setPointerEventsEnabled(true);
        });

    assertThrows(
        'setPointerEventsEnabled(false) after enterDocument must throw error.',
        () => {
          component.setPointerEventsEnabled(false);
        });
  },
});
