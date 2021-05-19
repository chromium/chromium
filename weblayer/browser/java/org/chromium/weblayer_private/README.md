# Which Context should I use?

The code in this directory references different types of contexts. Please read about what each
represents before deciding which one you should use.

## Embedder's Activity Context

The fragment that WebLayer is loaded in holds a reference to the activity that it is currently
attached to. This is what's referred to by [`mEmbedderActivityContext`][link1] in BrowserImpl and
BrowserFragmentImpl. It should be used to reference anything associated with the activity. For
instance, embedder-specific resources, like Color resources which are resolved according to the
theme of the embedding activity.

[link1]: https://source.chromium.org/chromium/chromium/src/+/6c336f4d55231595c038756f58a9e61d416a9c8f:weblayer/browser/java/org/chromium/weblayer_private/BrowserFragmentImpl.java;bpv=1;bpt=1

## WebLayer's Activity Context

WebLayer has a lot of resources of its own which need to be accessed by the implementation code. We
thus wrap the embedder's activity context so that resource and assert look-ups against the wrapped
context go to the WebView or WebLayer support APK and not the embedder's APK. This wrapped Context
is what's returned by [`BrowserImpl.getContext()`][link2]. Use this when referencing WebLayer specific
resources. This is expected to be the most common use case.

[link2]: https://source.chromium.org/chromium/chromium/src/+/main:weblayer/browser/java/org/chromium/weblayer_private/BrowserImpl.java?q=f:browserimpl%20getContext&ss=chromium%2Fchromium%2Fsrc

## Embedder's Application Context

Occasionally, we need the embedder's application context, as opposed to its activity context. For
instance, fetching the current locale which applies to the entire application.
Similar to WebLayer's Activity Context, this context is also wrapped in our implementation so we can
reference WebLayer-specific resources. This is what's returned by
[`ContextUtils.getApplicationContext()`][link3].
It shouldn't be downcast to Application (or any subclass thereof) since it's wrapped in a
ContextWrapper.

[link3]: https://source.chromium.org/chromium/chromium/src/+/main:base/android/java/src/org/chromium/base/ContextUtils.java?q=f:base%2FContextUtils%20getApplicationContext()&ss=chromium%2Fchromium%2Fsrc
