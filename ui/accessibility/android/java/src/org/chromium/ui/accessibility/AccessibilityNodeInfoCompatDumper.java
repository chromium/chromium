// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import static java.lang.String.CASE_INSENSITIVE_ORDER;

import android.graphics.Rect;
import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/** Utility class for turning AccessibilityNodeInfoCompat objects into strings for debugging. */
@NullMarked
public final class AccessibilityNodeInfoCompatDumper {
    private AccessibilityNodeInfoCompatDumper() {}

    /**
     * Helper method to perform a custom toString on a given AccessibilityNodeInfo object. Screen
     * size dependent attributes are excluded.
     *
     * @param node Object to create a toString for
     * @return String Custom toString result for the given object
     */
    public static String toString(AccessibilityNodeInfoCompat node) {
        return toString(node, false);
    }

    /**
     * Helper method to perform a custom toString on a given AccessibilityNodeInfo object.
     *
     * @param node Object to create a toString for
     * @param includeScreenSizeDependentAttributes Whether to include screen size dependent
     *     attributes
     * @return String Custom toString result for the given object
     */
    public static String toString(
            AccessibilityNodeInfoCompat node, boolean includeScreenSizeDependentAttributes) {
        if (node == null) return "";

        StringBuilder builder = new StringBuilder();

        // Print classname first, but only print content after the last period to remove redundancy.
        assert node.getClassName() != null : "Classname should never be null";
        String[] classNameParts = node.getClassName().toString().split("\\.");
        builder.append(classNameParts[classNameParts.length - 1]);

        // Print text unless it is empty (null is allowed).
        if (node.getText() == null) {
            builder.append(" text:\"null\"");
        } else if (!node.getText().toString().isEmpty()) {
            builder.append(" text:\"")
                    .append(node.getText().toString().replace("\n", "\\n"))
                    .append("\"");
        }
        // Print hint unless it is null or empty.
        if (node.getHintText() != null && !node.getHintText().toString().isEmpty()) {
            builder.append(" hint:\"").append(node.getHintText()).append("\"");
        }

        // Print tooltip text unless it is null or empty.
        if (node.getTooltipText() != null && !node.getTooltipText().toString().isEmpty()) {
            builder.append(" tooltipText:\"").append(node.getTooltipText()).append("\"");
        }

        // Text properties - Only print when non-null.
        if (node.getContentDescription() != null) {
            builder.append(" contentDescription:\"")
                    .append(node.getContentDescription().toString().replace("\n", "\\n"))
                    .append("\"");
        }
        if (node.getPaneTitle() != null) {
            builder.append(" paneTitle:\"").append(node.getPaneTitle()).append("\"");
        }
        if (node.getViewIdResourceName() != null) {
            builder.append(" viewIdResName:\"").append(node.getViewIdResourceName()).append("\"");
        }
        if (node.getError() != null) {
            builder.append(" error:\"").append(node.getError()).append("\"");
        }
        if (node.getStateDescription() != null
                && !node.getStateDescription().toString().isEmpty()) {
            builder.append(" stateDescription:\"").append(node.getStateDescription()).append("\"");
        }
        if (node.getContainerTitle() != null && !node.getContainerTitle().toString().isEmpty()) {
            builder.append(" containerTitle:\"").append(node.getContainerTitle()).append("\"");
        }
        if (node.getSupplementalDescription() != null
                && !node.getSupplementalDescription().toString().isEmpty()) {
            builder.append(" supplementalDescription:\"")
                    .append(node.getSupplementalDescription())
                    .append("\"");
        }

        // Boolean properties - Only print when set to true except for enabled and visibleToUser,
        // which are both mostly true, so only print when they are false.
        if (node.canOpenPopup()) {
            builder.append(" canOpenPopUp");
        }
        if (node.isCheckable()) {
            builder.append(" checkable");
        }
        if (node.isClickable()) {
            builder.append(" clickable");
        }
        if (node.isContentInvalid()) {
            builder.append(" contentInvalid");
        }
        if (node.isDismissable()) {
            builder.append(" dismissable");
        }
        if (node.isEditable()) {
            builder.append(" editable");
        }
        if (!node.isEnabled()) {
            builder.append(" disabled");
        }
        if (node.isFocusable()) {
            builder.append(" focusable");
        }
        if (node.isFocused()) {
            builder.append(" focused");
        }
        if (node.isMultiLine()) {
            builder.append(" multiLine");
        }
        if (node.isPassword()) {
            builder.append(" password");
        }
        if (node.isScrollable() && includeScreenSizeDependentAttributes) {
            builder.append(" scrollable");
        }
        if (node.isSelected()) {
            builder.append(" selected");
        }
        if (!node.isVisibleToUser()) {
            builder.append(" notVisibleToUser");
        }

        if (node.isFieldRequired()) {
            builder.append(" required");
        }

        if (node.isHeading()) {
            builder.append(" heading");
        }

        // Integer properties - Only print when not default values.
        if (node.getInputType() != InputType.TYPE_NULL) {
            builder.append(" inputType:").append(node.getInputType());
        }
        if (node.getTextSelectionStart() != -1) {
            builder.append(" textSelectionStart:").append(node.getTextSelectionStart());
        }
        if (node.getTextSelectionEnd() != -1) {
            builder.append(" textSelectionEnd:").append(node.getTextSelectionEnd());
        }
        if (node.getMaxTextLength() != -1) {
            builder.append(" maxTextLength:").append(node.getMaxTextLength());
        }
        if (node.getLiveRegion() != 0) {
            builder.append(" liveRegion:").append(node.getLiveRegion());
        }
        if (node.getExpandedState() != AccessibilityNodeInfo.EXPANDED_STATE_UNDEFINED) {
            builder.append(" expandedState:").append(node.getExpandedState());
        }
        if (node.getChecked() == AccessibilityNodeInfo.CHECKED_STATE_TRUE) {
            builder.append(" checked");
        } else if (node.getChecked() == AccessibilityNodeInfo.CHECKED_STATE_PARTIAL) {
            builder.append(" partiallyChecked");
        }

        // Child objects - print for non-null cases.
        if (node.getCollectionInfo() != null) {
            builder.append(" CollectionInfo:").append(toString(node.getCollectionInfo()));
        }
        if (node.getCollectionItemInfo() != null) {
            builder.append(" CollectionItemInfo:")
                    .append(toString(node, node.getCollectionItemInfo()));
        }
        if (node.getRangeInfo() != null) {
            builder.append(" RangeInfo:").append(toString(node.getRangeInfo()));
        }

        // Actions and Bundle extras - Always print.
        builder.append(" actions:")
                .append(toString(node.getActionList(), includeScreenSizeDependentAttributes));
        builder.append(" bundle:")
                .append(toString(node.getExtras(), includeScreenSizeDependentAttributes));

        // Add bounds when including screen size dependent attributes.
        if (includeScreenSizeDependentAttributes) {
            Rect output = new Rect();
            node.getBoundsInScreen(output);
            builder.append(" bounds:[")
                    .append(output.left)
                    .append(", ")
                    .append(output.top)
                    .append(" - ")
                    .append(output.width())
                    .append("x")
                    .append(output.height())
                    .append("]");

            output = new Rect();
            node.getBoundsInParent(output);
            builder.append(" boundsInParent:[")
                    .append(output.left)
                    .append(", ")
                    .append(output.top)
                    .append(" - ")
                    .append(output.width())
                    .append("x")
                    .append(output.height())
                    .append("]");
        }

        return builder.toString();
    }

    // Various helper methods to print custom toStrings for objects.
    private static String toString(AccessibilityNodeInfoCompat.CollectionInfoCompat info) {
        String prefix = "[";
        if (info.isHierarchical()) {
            prefix += "hierarchical, ";
        }
        if (info.getSelectionMode()
                != AccessibilityNodeInfoCompat.CollectionInfoCompat.SELECTION_MODE_NONE) {
            prefix +=
                    (info.getSelectionMode()
                                    == AccessibilityNodeInfoCompat.CollectionInfoCompat
                                            .SELECTION_MODE_MULTIPLE
                            ? "selection_mode_multiple, "
                            : "selection_mode_single, ");
        }
        return String.format(
                "%srows=%s, cols=%s]", prefix, info.getRowCount(), info.getColumnCount());
    }

    private static String toString(
            AccessibilityNodeInfoCompat info,
            AccessibilityNodeInfoCompat.CollectionItemInfoCompat collectionItemInfo) {
        String prefix = "[";
        if (info.isHeading()) {
            prefix += "tableHeader, ";
        }
        if (collectionItemInfo.isSelected()) {
            prefix += "selected, ";
        }

        int sortDirection = collectionItemInfo.getSortDirection();
        if (sortDirection
                != AccessibilityNodeInfoCompat.CollectionItemInfoCompat.SORT_DIRECTION_NONE) {
            String sortStr = "";
            if (sortDirection
                    == AccessibilityNodeInfoCompat.CollectionItemInfoCompat
                            .SORT_DIRECTION_ASCENDING) {
                sortStr = "ascending";
            } else if (sortDirection
                    == AccessibilityNodeInfoCompat.CollectionItemInfoCompat
                            .SORT_DIRECTION_DESCENDING) {
                sortStr = "descending";
            } else if (sortDirection
                    == AccessibilityNodeInfoCompat.CollectionItemInfoCompat.SORT_DIRECTION_OTHER) {
                sortStr = "other";
            }

            if (!sortStr.isEmpty()) {
                prefix += "sortDirection=" + sortStr + ", ";
            }
        }

        if (collectionItemInfo.getRowSpan() != 1) {
            prefix += String.format("rowSpan=%s, ", collectionItemInfo.getRowSpan());
        }
        if (collectionItemInfo.getColumnSpan() != 1) {
            prefix += String.format("colSpan=%s, ", collectionItemInfo.getColumnSpan());
        }
        return String.format(
                "%srowIndex=%s, colIndex=%s]",
                prefix, collectionItemInfo.getRowIndex(), collectionItemInfo.getColumnIndex());
    }

    private static String toString(AccessibilityNodeInfoCompat.RangeInfoCompat info) {
        return String.format(
                "[current=%s, min=%s, max=%s]", info.getCurrent(), info.getMin(), info.getMax());
    }

    private static String toString(
            List<AccessibilityNodeInfoCompat.AccessibilityActionCompat> actionList,
            boolean includeScreenSizeDependentAttributes) {
        actionList.sort((a1, b2) -> Integer.compare(a1.getId(), b2.getId()));

        List<String> actionStrings = new ArrayList<String>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (AccessibilityNodeInfoCompat.AccessibilityActionCompat action : actionList) {
            // Five actions are set on all nodes, so ignore those when printing the tree.
            if (action.equals(
                            AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                    .ACTION_NEXT_HTML_ELEMENT)
                    || action.equals(
                            AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                    .ACTION_PREVIOUS_HTML_ELEMENT)
                    || action.equals(
                            AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                    .ACTION_SHOW_ON_SCREEN)
                    || action.equals(
                            AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                    .ACTION_CONTEXT_CLICK)
                    || action.equals(
                            AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                    .ACTION_LONG_CLICK)) {
                continue;
            }

            // When |includeScreenSizeDependentAttributes| is false, filter out screen-size
            // dependent actions.
            if (!includeScreenSizeDependentAttributes) {
                // Scroll actions are dependent on screen size, so ignore them to reduce flakiness
                if (action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SCROLL_FORWARD)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SCROLL_BACKWARD)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SCROLL_DOWN)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SCROLL_UP)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SCROLL_RIGHT)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SCROLL_LEFT)) {
                    continue;
                }
                // Page actions are dependent on screen size, so ignore them to reduce flakiness.
                if (action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_PAGE_UP)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_PAGE_DOWN)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_PAGE_LEFT)
                        || action.equals(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_PAGE_RIGHT)) {
                    continue;
                }
            }

            actionStrings.add(toString(action.getId()));
        }
        builder.append(TextUtils.join(", ", actionStrings)).append("]");

        return builder.toString();
    }

    public static String toString(int action) {
        if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat
                        .ACTION_NEXT_AT_MOVEMENT_GRANULARITY
                        .getId()) {
            return "NEXT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat
                        .ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY
                        .getId()) {
            return "PREVIOUS";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_TEXT.getId()) {
            return "SET_TEXT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PASTE.getId()) {
            return "PASTE";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_IME_ENTER.getId()) {
            return "IME_ENTER";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_SELECTION
                        .getId()) {
            return "SET_SELECTION";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CUT.getId()) {
            return "CUT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COPY.getId()) {
            return "COPY";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_FORWARD
                        .getId()) {
            return "SCROLL_FORWARD";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_BACKWARD
                        .getId()) {
            return "SCROLL_BACKWARD";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_UP.getId()) {
            return "SCROLL_UP";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_UP.getId()) {
            return "PAGE_UP";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_DOWN
                        .getId()) {
            return "SCROLL_DOWN";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_DOWN.getId()) {
            return "PAGE_DOWN";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_LEFT
                        .getId()) {
            return "SCROLL_LEFT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_LEFT.getId()) {
            return "PAGE_LEFT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SCROLL_RIGHT
                        .getId()) {
            return "SCROLL_RIGHT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_PAGE_RIGHT
                        .getId()) {
            return "PAGE_RIGHT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLEAR_FOCUS
                        .getId()) {
            return "CLEAR_FOCUS";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_FOCUS.getId()) {
            return "FOCUS";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat
                        .ACTION_CLEAR_ACCESSIBILITY_FOCUS
                        .getId()) {
            return "CLEAR_AX_FOCUS";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_ACCESSIBILITY_FOCUS
                        .getId()) {
            return "AX_FOCUS";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_CLICK.getId()) {
            return "CLICK";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_EXPAND.getId()) {
            return "EXPAND";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_COLLAPSE.getId()) {
            return "COLLAPSE";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SET_PROGRESS
                        .getId()) {
            return "SET_PROGRESS";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_LONG_CLICK
                        .getId()) {
            return "LONG_CLICK";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_NEXT_HTML_ELEMENT
                        .getId()) {
            return "NEXT_HTML_ELEMENT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat
                        .ACTION_PREVIOUS_HTML_ELEMENT
                        .getId()) {
            return "PREVIOUS_HTML_ELEMENT";
        } else if (action
                == AccessibilityNodeInfoCompat.AccessibilityActionCompat.ACTION_SHOW_ON_SCREEN
                        .getId()) {
            return "SHOW_ON_SCREEN";
        } else {
            return "NOT_IMPLEMENTED";
        }
    }

    private static String toString(Bundle extras, boolean includeScreenSizeDependentAttributes) {
        List<String> sortedKeySet = new ArrayList<String>(extras.keySet());
        sortedKeySet.sort(CASE_INSENSITIVE_ORDER);
        List<String> bundleStrings = new ArrayList<>();
        StringBuilder builder = new StringBuilder();
        builder.append("[");
        for (String key : sortedKeySet) {
            // Ignore screen size dependent attributes if not requested.
            if (!includeScreenSizeDependentAttributes) {
                if (key.equals("AccessibilityNodeInfo.unclippedTop")
                        || key.equals("AccessibilityNodeInfo.unclippedBottom")
                        || key.equals("AccessibilityNodeInfo.unclippedLeft")
                        || key.equals("AccessibilityNodeInfo.unclippedRight")
                        || key.equals("AccessibilityNodeInfo.unclippedWidth")
                        || key.equals("AccessibilityNodeInfo.unclippedHeight")
                        || key.equals("AccessibilityNodeInfo.offscreen")) {
                    continue;
                }
            }

            Object value = extras.get(key);
            if (value == null || value.toString().isEmpty()) {
                continue;
            }

            // For the special case of the supported HTML elements, which prints the same for the
            // rootWebArea on each test, assert consistency and suppress from results.
            if (key.equals("ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES")) {
                continue;
            }

            if (key.contains("AccessibilityNodeInfoCompat")) {
                continue;
            }

            if (key.equals("AccessibilityNodeInfo.cssDisplay")) {
                continue;
            }

            bundleStrings.add(key.replace("AccessibilityNodeInfo.", "") + "=\"" + value + "\"");
        }
        builder.append(TextUtils.join(", ", bundleStrings)).append("]");

        return builder.toString();
    }
}
