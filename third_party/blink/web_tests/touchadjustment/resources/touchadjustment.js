/* function for finding the absolute bounds of a node (both inline and block) */
function findAbsoluteBounds(node)
{
    var bounds = node.getBoundingClientRect();
    return {
        left: bounds.left,
        top: bounds.top,
        width: bounds.right - bounds.left,
        height: bounds.bottom - bounds.top
    };
}

function nodeToString(node)
{
    if (node === undefined)
        return 'undefined';
    if (node === null)
        return 'null';
    if (!node.nodeName)
        return 'not a node';
    if (node.nodeType == 3)
        return "'"+node.nodeValue+"'";
    return node.nodeName + (node.id ? ('#' + node.id) : '');
}

function boundsToString(bounds)
{
    return "("+bounds.left+","+bounds.top+")x("+bounds.width+","+bounds.height+")";
}

function pointToString(point)
{
    return "("+point.x+","+point.y+")";
}


function shouldBeNode(adjustedNode, targetNode) {
    if (typeof targetNode == "string") {
        var adjustedNodeString = nodeToString(adjustedNode);
        if (targetNode == adjustedNodeString) {
            testPassed("adjusted node was " + targetNode + ".");
        }
        else {
            testFailed("adjusted node should be " + targetNode  + ". Was " + adjustedNodeString + ".");
        }
        return;
    }
    if (targetNode == adjustedNode) {
        testPassed("adjusted node was " + nodeToString(targetNode) + ".");
    }
    else {
        testFailed("adjusted node should be " + nodeToString(targetNode)  + ". Was " + nodeToString(adjustedNode) + ".");
    }
}

function shouldBeWithin(adjustedPoint, targetArea) {
    if (adjustedPoint.x >= targetArea.left && adjustedPoint.y >= targetArea.top
        && adjustedPoint.x <= (targetArea.left + targetArea.width)
        && adjustedPoint.y <= (targetArea.top + targetArea.height)) {
        testPassed("adjusted point was within " + boundsToString(targetArea));
    } else {
        testFailed("adjusted node should be within " + boundsToString(targetArea)  + ". Was " + pointToString(adjustedPoint));
    }
}

function shadowHost(node)
{
    for (; node != null; node = node.parentNode) {
        if (node.host)
            return node.host;
    }
    return node;
}

function testTouchPoint(touchpoint, targetNode, allowTextNodes, disallowShadowDOM)
{
    var adjustedNode = internals.touchNodeAdjustedToBestClickableNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    if (!allowTextNodes && adjustedNode && adjustedNode.nodeType == 3)
        adjustedNode = adjustedNode.parentNode;
    if (disallowShadowDOM && adjustedNode && adjustedNode.nodeType == 1) {
        while (shadowHost(adjustedNode))
            adjustedNode = shadowHost(adjustedNode);
    }
    shouldBeNode(adjustedNode, targetNode);
}

function testTouchPointContextMenu(touchpoint, targetNode, allowTextNodes)
{
    var adjustedNode = internals.touchNodeAdjustedToBestContextMenuNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    if (!allowTextNodes && adjustedNode && adjustedNode.nodeType == 3)
        adjustedNode = adjustedNode.parentNode;
    shouldBeNode(adjustedNode, targetNode);
}

function testTouchPointElementForStylusWritable(touchpoint, targetNode, allowTextNodes, disallowShadowDOM)
{
    var adjustedNode = internals.touchNodeAdjustedToBestStylusWritableNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    if (!allowTextNodes && adjustedNode && adjustedNode.nodeType == 3)
        adjustedNode = adjustedNode.parentNode;
    if (disallowShadowDOM && adjustedNode && adjustedNode.nodeType == 1) {
        while (shadowHost(adjustedNode))
            adjustedNode = shadowHost(adjustedNode);
    }
    shouldBeNode(adjustedNode, targetNode);
}

function adjustTouchPoint(touchpoint)
{
    var adjustedPoint = internals.touchPositionAdjustedToBestClickableNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    return adjustedPoint;
}

function adjustTouchPointContextMenu(touchpoint)
{
    var adjustedPoint = internals.touchPositionAdjustedToBestContextMenuNode(touchpoint.left, touchpoint.top, touchpoint.width, touchpoint.height, document);
    return adjustedPoint;
}

function touchPoint(x, y, radiusX, radiusY)
{
    if (!radiusY)
        radiusY = radiusX;

    return {
        left: x - radiusX,
        top: y - radiusY,
        width: radiusX * 2,
        height: radiusY * 2,
        get x() { return this.left + this.width/2; },
        get y() { return this.top + this.height/2; }
    };
}

function offsetTouchPoint(bounds, relativePosition, touchOffset, touchRadiusX, touchRadiusY)
{
    if (!touchRadiusX)
        touchRadiusX = 1;
    if (!touchRadiusY)
        touchRadiusY = touchRadiusX;

    // Start with the center of the touch at the top-left of the bounds.
    var touchpoint = touchPoint(bounds.left, bounds.top, touchRadiusX, touchRadiusY);
 
    // Adjust the touch point as requested.
    switch (relativePosition) {
    case 'center':
        touchpoint.left += bounds.width / 2;
        touchpoint.top += bounds.height / 2;
        break;
    case 'left':
        touchpoint.left -= touchOffset;
        touchpoint.top += bounds.height / 2;
        break;
    case 'right':
        touchpoint.left += bounds.width + touchOffset;
        touchpoint.top +=  bounds.height / 2;
        break;
    case 'top-left':
        touchpoint.left -= touchOffset;
        touchpoint.top -= touchOffset;
        break;
    case 'top-right':
        touchpoint.left += bounds.width + touchOffset;
        touchpoint.top -= touchOffset;
        break;
    case 'bottom-left':
        touchpoint.left -= touchOffset;
        touchpoint.top += bounds.height + touchOffset;
        break;
    case 'bottom-right':
        touchpoint.left += bounds.width + touchOffset;
        touchpoint.top += bounds.height + touchOffset;
        break;
    case 'top':
        touchpoint.left += bounds.width / 2;
        touchpoint.top -= touchOffset;
        break;
    case 'bottom':
        touchpoint.left += bounds.width / 2;
        touchpoint.top += bounds.height + touchOffset;
    }

    return touchpoint;
}
