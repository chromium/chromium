// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.chromium.ui.animation.TranslationAnimatorFactory.buildTranslationAnimation;

import android.animation.Animator;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

/** Factory class for creating translation animations for {@link Drawable} elements. */
public class DrawableTranslationAnimatorFactory {
    /**
     * Creates an animation for translations. The animator will begin at the maximum displacement
     * specified and will return to a the non-translated position.
     *
     * @param drawable The {@link Drawable} to be translated in the animation.
     * @param dX The initial amount the drawable is to be translated horizontally (in px).
     * @param dY The initial amount the drawable is translated vertically (in px).
     */
    public static Animator build(Drawable drawable, int dX, int dY) {
        Rect initialBounds = drawable.copyBounds();
        Animator animator =
                buildTranslationAnimation(
                        dX, dY, (x, y) -> translateDrawable(drawable, initialBounds, x, y));
        drawable.setBounds(initialBounds);
        return animator;
    }

    /**
     * Translates a {@link Drawable} by a specified displacement.
     *
     * @param drawable The drawable to be translated.
     * @param initialBounds The {@link Rect} containing the initial bounds of {@param drawable}
     *     prior to any animation.
     * @param dX The amount the drawable is to be translated horizontally (in px).
     * @param dY The amount the drawable is translated vertically (in px).
     */
    private static void translateDrawable(
            Drawable drawable, Rect initialBounds, float dX, float dY) {
        Rect currentBounds = drawable.copyBounds();
        int newLeft = (int) (initialBounds.left + dX);
        int newTop = (int) (initialBounds.top + dY);
        int newRight = newLeft + drawable.getIntrinsicWidth();
        int newBottom = newTop + drawable.getIntrinsicHeight();
        currentBounds.set(newLeft, newTop, newRight, newBottom);
        drawable.setBounds(currentBounds);
    }
}
