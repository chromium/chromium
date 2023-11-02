// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Extra optional parameters for {@link NavigationController#navigate}.
 *
 * Default values should be kept in sync with C++ NavigationController::LoadURLParams.
 *
 * @since 83
 */
public class NavigateParams implements Parcelable {
    public boolean mShouldReplaceCurrentEntry;

    public static final Parcelable.Creator<NavigateParams> CREATOR =
            new Parcelable.Creator<NavigateParams>() {
                @Override
                public NavigateParams createFromParcel(Parcel in) {
                    return new NavigateParams(in);
                }

                @Override
                public NavigateParams[] newArray(int size) {
                    return new NavigateParams[size];
                }
            };

    public NavigateParams() {}

    private NavigateParams(Parcel in) {
        readFromParcel(in);
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(mShouldReplaceCurrentEntry ? 1 : 0);
    }

    public void readFromParcel(Parcel in) {
        mShouldReplaceCurrentEntry = in.readInt() == 1;
    }

    @Override
    public int describeContents() {
        return 0;
    }
}
