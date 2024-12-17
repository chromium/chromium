// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import org.chromium.build.annotations.NullMarked;

/** Classes that hold an instance of {@link ModalDialogManager} should implement this interface. */
@NullMarked
public interface ModalDialogManagerHolder {
    /**
     * @return The {@link ModalDialogManager} associated with this class
     */
    ModalDialogManager getModalDialogManager();
}
