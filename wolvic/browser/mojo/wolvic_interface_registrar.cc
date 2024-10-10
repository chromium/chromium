#include "wolvic/browser/mojo/wolvic_interface_registrar.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "wolvic/jni_headers/WolvicInterfaceRegistrar_jni.h"

void RegisterWolvicJavaMojoInterfaces() {
  Java_WolvicInterfaceRegistrar_registerMojoInterfaces(
      base::android::AttachCurrentThread());
}
