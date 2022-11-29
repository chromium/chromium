# WebEngine public API

This directory contains the public API for WebEngine. WebEngine provides both a
C++ and Java API. Note that while WebEngine's implementation builds on top of
//content, its public API does *not* expose the Content API.

Note: The WebEngine API is still under development and should not be depended
upon directly. It will become available through an Android Jetpack Library.

## Java API

In general, the Java API is a thin veneer over the C++ API. For the most part,
functionality should be added to the C++ side with the Java implementation
calling into it. Where the two APIs diverge is for platform specific
functionality. For example, the Java API uses Android Fragments, which do not
apply to the C++ side.

The public API should follow the Android API guidelines
(https://goto.google.com/android-api-guidelines). This results in naming
differences between the C++ and Java code. For example, NewTabDelegate in C++
vs NewTabCallback in Java.

One of the design constraints of WebEngine's Java implementation is that we do
not want embedders to ship their own copy of "//content". Instead, the
implementation is loaded from the WebView APK (not the Chrome APK, because the
WebView APK is available on more devices). This constraint results in the Java
implementation consisting of three distinct parts.

### Java client library

Code lives in "//weblayer/public/java". This is the code used by embedders. The
client library contains very little logic, rather it delegates to the Java
implementation over AIDL.

This code should not have any dependencies on any other code in the chrome repo.

The embedders can use the API surface defined in the `org.chromium.webengine`
package. It spins up an Android Service running the code in the
`org.chromium.weblayer` package. The Service is responsible for loading the
implementation from the WebView APK.

### Java AIDL

This is best thought of as WebEngine's ABI (for Java).

The client library loads the WebEngine implementation from WebView APK and uses
AIDL for the IPC. The aidl interfaces are defined in
"//weblayer/browser/java/org/chromium/weblayer_private/interfaces". AIDL is used
to enable the implementation to be loaded using a different ClassLoader than
the embedder.

In general, any interface ending with 'Client' means the interface is from the
implementation side to the client library. An interface without 'Client' is
from the client to the implementation.

### Java implementation

This is often referred to as the 'implementation' of the Java API. This is
where the bulk of the Java code lives and sits on top of the C++ API. The code
for this lives in "//weblayer/browser/java".
