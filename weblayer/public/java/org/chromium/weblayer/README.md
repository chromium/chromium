# WebEngine's Service Java API

See comments in [weblayer/public/README.md](../../../../README.md) for
general comments on WebEngine's public API and architecture. This document
provides guidance on writing WebEngine's public Java API.

## Typical method

A method typically looks something like:

```
public void aMethod() {
  // Nearly all methods must be called on the ui thread.
  ThreadCheck.ensureOnUiThread();
  // Newly added functions have version checks.
  if (WebLayer.getSupportedMajorVersionInternal() < 86) {
    throw new UnsupportedOperationException();
  }
  // For cases where the implementation may be destroyed, throw an exception
  // to help debugging.
  if (mImpl == null) {
    throw new IllegalStateException("Attempt to use class after destroyed");
  }
  // All AIDL calls have checked exceptions. use try/catch and rethrow an
  // APICallException.
  try {
    mImpl.doSomething();
  } catch (RemoteException e) {
    throw new APICallException(e);
  }
}
```

## Versioning

A more complete write up is here
https://docs.google.com/document/d/1RsknOu7YOl2AWVXri5kfl7QblgGf41A3ZtJWJoBZ-4I.

In general, only add new methods, and do not modify signatures of existing
methods. This is especially important of AIDL files, but also applies to the
signature of methods in the public API.

Adding a new method to the client library generally looks like:

```
  /**
   * @since 86
   */
  public void aNewMethod() {
    ThreadCheck.ensureOnUiThread();
    if (WebLayer.getSupportedMajorVersionInternal() < 86) {
        throw new UnsupportedOperationException();
    }
  }
}
```

The implementation may also need to check the version of the client. This can
be done using `WebLayerFactoryImpl.getClientMajorVersion()`.

## Avoid Parcelable

Parcelable does not support versioning. Additionally, as all function arguments
are written to the same Parcel, it would be very challenging to layer versioning
on top of Parcelable. For these reasons, WebEngine's public API avoids using
Parcelable.

For methods that would have a significant number of arguments, use an Object.
At the AIDL boundary, pass all members of the Object as arguments. This makes
it trivial to add new parameters later on.

For example,

```
public class CustomParameter {
  private String mArg1;
  private String mArg2;
  public String getArgument1() { return mArg1; }
  public String getArgument2() { return mArg2; }
}
```

And calling the AIDL method would look like:

```
  aidlMethod(customParameter.getArgument1(), customParameter.getArgument2());
```

## Abstract classes

When adding a new method to an abstract class it must be concrete. To do
otherwise would mean embedders can not compile their code against a new version
of the client library without changing their code.
