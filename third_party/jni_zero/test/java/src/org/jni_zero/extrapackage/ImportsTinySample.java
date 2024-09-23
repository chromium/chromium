package org.jni_zero.extrapackage;

import org.jni_zero.TinySample;

class ImportsTinySample {
    TinySample a;

    @NativeMethods()
    interface Natives {
        void asdf();
    }
}
