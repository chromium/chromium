// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.accessibilityservice.AccessibilityService;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.base.Log;
import org.chromium.ui.accessibility.AccessibilityNodeInfoCompatDumper;

import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import javax.annotation.concurrent.GuardedBy;

public class AccessibilityTestService extends AccessibilityService {
    private static final String TAG = "A11yTestService";

    // Extended selection offset types, defined in:
    // androidx.view.accessibility.AccessibilityNodeInfoCompat
    private static final int OFFSET_TYPE_TEXT = 0;
    private static final int OFFSET_TYPE_CHILD = 1;

    private static final String EXTRA_SELECTION_START_OFFSET_TYPE =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SELECTION_START_OFFSET_TYPE";
    private static final String EXTRA_SELECTION_END_OFFSET_TYPE =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SELECTION_END_OFFSET_TYPE";

    private static AccessibilityTestService sInstance;
    private static final Object sLock = new Object();

    public interface AccessibilityServiceListener {
        default void onAccessibilityEvent(AccessibilityEvent event) {}

        default boolean shouldCacheEvent(AccessibilityEvent event) {
            return true;
        }
    }

    @GuardedBy("sLock")
    private static AccessibilityServiceListener sListener;

    @GuardedBy("sLock")
    private static final List<AccessibilityEvent> sEventCache = new ArrayList<>();

    public static AccessibilityTestService getInstance() {
        return sInstance;
    }

    public static boolean tryWaitFor(WaitForParams params) {
        CompletableFuture<Boolean> future = new CompletableFuture<>();
        final boolean hasEventMatcher = params.eventMatcher != null;
        final boolean hasNodeMatcher = params.nodeMatcher != null;

        if (!hasEventMatcher && !hasNodeMatcher) {
            Log.e(TAG, "Neither event nor node params are set in tryWaitFor");
            return false;
        }

        final boolean initialEventReceived;
        synchronized (sLock) {
            clearListenerLocked();

            if (hasEventMatcher) {
                if (searchAndConsumeEventCacheLocked(params.eventMatcher)) {
                    Log.i(TAG, "Found event in cache.");
                    if (hasNodeMatcher) {
                        if (findNode(
                                params.nodeMatcher,
                                "Found node in tree immediately after event cache match.")) {
                            return true;
                        }
                        initialEventReceived = true;
                    } else {
                        return true;
                    }
                } else {
                    initialEventReceived = false;
                }
            } else {
                if (findNode(params.nodeMatcher, "Found node in tree immediately.")) {
                    return true;
                }
                initialEventReceived = true;
            }
        }

        if (hasEventMatcher) {
            synchronized (sLock) {
                // We only clear the cache as long as we are matching an event which we didn't find
                // in the cache.
                clearEventCacheLocked();
            }
        }

        final java.util.concurrent.atomic.AtomicBoolean eventReceived =
                new java.util.concurrent.atomic.AtomicBoolean(initialEventReceived);

        AccessibilityServiceListener listener =
                new AccessibilityServiceListener() {
                    @Override
                    public void onAccessibilityEvent(AccessibilityEvent event) {
                        synchronized (sLock) {
                            if (!eventReceived.get()) {
                                if (eventMatches(event, params.eventMatcher)) {
                                    Log.i(TAG, "  Event MATCHED.");
                                    eventReceived.set(true);
                                    if (hasNodeMatcher) {
                                        if (findNode(
                                                params.nodeMatcher,
                                                "Found node in tree after event.")) {
                                            future.complete(true);
                                        }
                                    } else {
                                        future.complete(true);
                                    }
                                }
                            } else {
                                if (hasNodeMatcher) {
                                    if (findNode(params.nodeMatcher, "Found node in tree.")) {
                                        future.complete(true);
                                    }
                                }
                            }
                        }
                    }

                    @Override
                    public boolean shouldCacheEvent(AccessibilityEvent event) {
                        if (hasEventMatcher && !hasNodeMatcher) {
                            return false;
                        }
                        // If we're matching both an event and a node, we only should cache incoming
                        // events as long as the desired event has been received.
                        if (hasEventMatcher && hasNodeMatcher) {
                            return eventReceived.get();
                        }
                        return true;
                    }
                };

        synchronized (sLock) {
            setListenerLocked(listener);
        }

        try {
            return future.get(params.timeoutMs, TimeUnit.MILLISECONDS);
        } catch (TimeoutException e) {
            Log.w(TAG, "Timed out waiting");
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error waiting", e);
            return false;
        } finally {
            synchronized (sLock) {
                clearListenerLocked();
            }
        }
    }

    private static boolean findNode(NodeMatcher nodeMatcher, String infoLog) {
        AccessibilityTestService instance = getInstance();
        if (instance == null) {
            Log.e(TAG, "AccessibilityTestService's instance was null when looking after node.");
            return false;
        }
        AccessibilityNodeInfo root = instance.getRootInActiveWindow();
        if (root == null) return false;
        Log.i(TAG, infoLog);
        return findNodeRecursive(root, nodeMatcher) != null;
    }

    private static AccessibilityNodeInfo findNodeRecursive(
            AccessibilityNodeInfo node, NodeMatcher nodeMatcher) {
        if (node == null) return null;

        CharSequence nodeClassName = node.getClassName();
        CharSequence nodeText = node.getText();
        Log.i(TAG, "  findNodeRecursive: " + nodeClassName + " - " + nodeText);

        if (nodeMatches(node, nodeMatcher)) {
            return node;
        }

        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfo child = node.getChild(i);
            AccessibilityNodeInfo result = findNodeRecursive(child, nodeMatcher);
            if (result != null) {
                return result;
            }
        }
        return null;
    }

    private static boolean nodeMatches(AccessibilityNodeInfo node, NodeMatcher nodeMatcher) {
        CharSequence nodeClassName = node.getClassName();
        CharSequence nodeText = node.getText();

        boolean classNameMatches =
                TextUtils.isEmpty(nodeMatcher.className)
                        || TextUtils.equals(nodeClassName, nodeMatcher.className);
        boolean textMatches =
                TextUtils.isEmpty(nodeMatcher.text) || TextUtils.equals(nodeText, nodeMatcher.text);
        boolean inputFocusedMatches =
                !nodeMatcher.hasInputFocused || (node.isFocused() == nodeMatcher.inputFocused);
        boolean accessibilityFocusedMatches =
                !nodeMatcher.hasAccessibilityFocused
                        || (node.isAccessibilityFocused() == nodeMatcher.accessibilityFocused);

        return classNameMatches
                && textMatches
                && inputFocusedMatches
                && accessibilityFocusedMatches;
    }

    @GuardedBy("sLock")
    public static void setListenerLocked(AccessibilityServiceListener listener) {
        if (sListener != null && listener != null) {
            Log.e(TAG, "Listener already set!");
        }
        sListener = listener;
    }

    @GuardedBy("sLock")
    public static void clearListenerLocked() {
        sListener = null;
    }

    @GuardedBy("sLock")
    public static boolean searchAndConsumeEventCacheLocked(EventMatcher eventMatcher) {
        ListIterator<AccessibilityEvent> iterator = sEventCache.listIterator();
        int foundIndex = -1;
        while (iterator.hasNext()) {
            int index = iterator.nextIndex();
            AccessibilityEvent event = iterator.next();
            if (eventMatches(event, eventMatcher)) {
                foundIndex = index;
                break;
            }
        }

        if (foundIndex != -1) {
            sEventCache.subList(0, foundIndex + 1).clear();
            return true;
        }
        return false;
    }

    @GuardedBy("sLock")
    public static void clearEventCacheLocked() {
        sEventCache.clear();
    }

    public static boolean tryPerformActionOnNode(
            NodeMatcher matcher, int action, Bundle arguments) {
        synchronized (sLock) {
            AccessibilityTestService instance = sInstance;
            if (instance == null) {
                Log.e(TAG, "AccessibilityTestService instance is null");
                return false;
            }

            AccessibilityNodeInfo root = instance.getRootInActiveWindow();
            if (root == null) {
                Log.e(TAG, "Root node is null");
                return false;
            }

            AccessibilityNodeInfo targetNode = findNodeRecursive(root, matcher);

            if (targetNode != null) {
                Log.i(TAG, "Found node: " + targetNode.toString());
                if (arguments != null) {
                    return targetNode.performAction(action, arguments);
                } else {
                    return targetNode.performAction(action);
                }
            }

            Log.e(TAG, "Node not found");
            return false;
        }
    }

    public static String dumpWebContentsAccessibilityTree() {
        synchronized (sLock) {
            AccessibilityTestService instance = sInstance;
            if (instance == null) {
                Log.e(TAG, "AccessibilityTestService instance is null");
                return "Error: AccessibilityTestService instance is null";
            }

            AccessibilityNodeInfo root = instance.getRootInActiveWindow();
            if (root == null) {
                Log.e(TAG, "Root node is null");
                return "Error: Root node is null";
            }

            AccessibilityNodeInfoCompat a11yFocusNode = null;
            AccessibilityNodeInfo a11yFocus =
                    instance.findFocus(AccessibilityNodeInfo.FOCUS_ACCESSIBILITY);
            if (a11yFocus != null) {
                a11yFocusNode = AccessibilityNodeInfoCompat.wrap(a11yFocus);
            }

            AccessibilityNodeInfoCompat inputFocusNode = null;
            AccessibilityNodeInfo inputFocus =
                    instance.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
            if (inputFocus != null) {
                inputFocusNode = AccessibilityNodeInfoCompat.wrap(inputFocus);
            }

            // Find the WebView node.
            NodeMatcher nodeMatcher = new NodeMatcher();
            nodeMatcher.className = "android.webkit.WebView";
            AccessibilityNodeInfo webViewNode = findNodeRecursive(root, nodeMatcher);
            if (webViewNode == null) {
                Log.e(TAG, "WebView node not found");
                return "Error: WebView node not found";
            }

            // Use the dumper utility to serialize the tree.
            return dumpSubtreeRecursive(
                    AccessibilityNodeInfoCompat.wrap(webViewNode),
                    "",
                    a11yFocusNode,
                    inputFocusNode);
        }
    }

    private static String dumpSubtreeRecursive(
            AccessibilityNodeInfoCompat node,
            String indent,
            AccessibilityNodeInfoCompat a11yFocusNode,
            AccessibilityNodeInfoCompat inputFocusNode) {
        if (node == null) return "";

        StringBuilder builder = new StringBuilder();
        builder.append(indent);
        builder.append(AccessibilityNodeInfoCompatDumper.toString(node));

        if (a11yFocusNode != null && node.equals(a11yFocusNode)) {
            builder.append(" isAccessibilityFocusedViaFindFocus");
        }
        if (inputFocusNode != null && node.equals(inputFocusNode)) {
            builder.append(" isInputFocusedViaFindFocus");
        }

        // Append extended selection information if available by checking ancestors.
        // Note that we can't stop at the first found selection, as content editables that are
        // exposing their subtrees may find a selection at the edit text field (root of the content
        // editable), and another one at the root of the web area (the one expected). The text
        // editable always reports a selection for backwards compatibility.
        AccessibilityNodeInfoCompat ancestor = node;
        while (ancestor != null) {
            AccessibilityNodeInfoCompat.SelectionCompat selection = ancestor.getSelection();
            // SelectionCompat might be generated by the Android platform framework (e.g. for
            // standard selection inside focused EditText fields). These OS populated selections
            // lack the offset type information and are not considered extended selections.
            int startOffsetType =
                    ancestor.getExtras().getInt(EXTRA_SELECTION_START_OFFSET_TYPE, -1);
            int endOffsetType = ancestor.getExtras().getInt(EXTRA_SELECTION_END_OFFSET_TYPE, -1);
            if (selection != null && startOffsetType != -1 && endOffsetType != -1) {
                AccessibilityNodeInfoCompat.SelectionPositionCompat start = selection.getStart();
                boolean addedSelectionInfo = false;
                if (start != null) {
                    if (node.equals(start.getNode())) {
                        builder.append(" extendedSelectionStart:")
                                .append(
                                        getExtendedSelectionString(
                                                start.getOffset(), startOffsetType));
                        addedSelectionInfo = true;
                    }
                }

                AccessibilityNodeInfoCompat.SelectionPositionCompat end = selection.getEnd();
                if (end != null) {
                    if (node.equals(end.getNode())) {
                        builder.append(" extendedSelectionEnd:")
                                .append(getExtendedSelectionString(end.getOffset(), endOffsetType));
                        addedSelectionInfo = true;
                    }
                }

                if (addedSelectionInfo) {
                    // Continue checking ancestors even if we found a selection, as multiple
                    // nodes might report selection for the same target (e.g. EditText and Root).
                    // However, if that selection was the one that pointed to the current node, it
                    // is safe to exit early.
                    break;
                }
            }
            AccessibilityNodeInfoCompat parent = ancestor.getParent();
            if (!ancestor.equals(node)) {
                ancestor.recycle();
            }
            ancestor = parent;
        }

        builder.append("\n");

        String childIndent = indent + "  ";
        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfoCompat child = node.getChild(i);
            builder.append(dumpSubtreeRecursive(child, childIndent, a11yFocusNode, inputFocusNode));
        }

        return builder.toString();
    }

    static boolean eventMatches(AccessibilityEvent event, EventMatcher eventMatcher) {
        if (event.getEventType() != eventMatcher.eventType) return false;

        // contentChangeTypes is a bitmask: when a non-zero value is provided, only match events
        // whose getContentChangeTypes() includes all requested flags. Other flags set by Android
        // are tolerated.
        if (eventMatcher.contentChangeTypes != 0
                && (event.getContentChangeTypes() & eventMatcher.contentChangeTypes)
                        != eventMatcher.contentChangeTypes) {
            return false;
        }

        AccessibilityNodeInfo source = event.getSource();

        if (eventMatcher.sourceMatcher != null) {
            if (source == null || !nodeMatches(source, eventMatcher.sourceMatcher)) {
                return false;
            }
        }

        return true;
    }

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();
        Log.d(TAG, "onServiceConnected");
        synchronized (sLock) {
            sInstance = this;
        }
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind");
        sInstance = null;
        synchronized (sLock) {
            clearListenerLocked();
            clearEventCacheLocked();
        }
        return super.onUnbind(intent);
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        Log.i(TAG, "onAccessibilityEvent: " + event);
        synchronized (sLock) {
            boolean shouldCache = true;
            if (sListener != null) {
                sListener.onAccessibilityEvent(event);
                shouldCache = sListener.shouldCacheEvent(event);
            }
            if (shouldCache) {
                sEventCache.add(AccessibilityEvent.obtain(event));
            }
        }
    }

    @Override
    public void onInterrupt() {}

    private static String getExtendedSelectionString(int offset, int offsetType) {
        return "{" + offset + ", " + (offsetType == OFFSET_TYPE_TEXT ? "text" : "child") + "}";
    }
}
