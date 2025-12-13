# Implementing Features As Unowned User Data

Features scoped to `BrowserWindowInterface` and `TabInterface` are created and
managed by objects like `BrowserWindowFeatures` and `TabFeatures`.

This leads to a situation where mock versions of `BrowserWindowInterface` and
`TabInterface` have no features or features object, making it challenging to
test production code that accesses features through that API.

Likewise, there had been no general API to inject just a single mocked feature
into an otherwise normal browser.

**Unowned User Data** is the solution for these cases.

Unowned User Data separates managing the creation and lifespan of your feature
from managing how your feature is retrieved in production code.

Management is still done by `TabFeatures` or `BrowserWindowFeatures`, but the
feature itself can be retrieved directly from `TabInterface` or
`BrowserWindowInterface`, which hold references but do not own them (hence,
"unowned user data").

This eliminates boilerplate and allows these references to be easily injected or
substituted in tests.

## Retrieving An Unowned User Data Feature Object in Production Code

Once your feature is set up to work with Unowned User Data, you can retrieve it
using the `From()` method:
```cpp
  TabInterface* tab;
  BrowserWindowInterface* browser;
  auto* lens_controller = LensSearchController::From(tab);
  auto* user_ed = BrowserUserEducationInterface::From(browser);
```

## Making Your Feature Work with Unowned User Data

Making your feature compatible with Unowned User Data takes ~7 lines of code:
 1. Include `DECLARE_USER_DATA(MyTabFeature)` at the top of your class
   declaration and `DEFINE_USER_DATA(MyTabFeature)` in your .cc file.
 1. Add and initialize a `ScopedUnownedUserData<MyFeature>` as a private data
   member.
 1. Add a static `MyTabFeature* From(TabInterface*)` method.

You can initialize the `ScopedUnownedUserData` as follows:
```cpp
  MyTabFeature(TabInterface* tab, ...)
    : scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this),
      ...
```

The `ScopedUnownedUserData` can be created in an `Init()` function that is
called later, if you do not have access to the tab or browser window on
creation. The only requirement is that doesn't outlive your feature object or
the tab or browser window.

`From()` is just a convenience method:
```cpp
  static MyTabFeature* From(TabInterface* tab) {
    return Get(tab->GetUnownedUserDataHost());
  }
```

## Unowned User Data in Unit Tests

Say you want to test your tab feature in a unit test with a `MockTabInterface`
rather than a real one. You can give the mock an `UnownedUserDataHost` and then
attach your feature object to the mock tab:

```cpp
  void SetUp() override {
    // Make sure the mock tab uses a real data host.
    EXPECT_CALL(
        mock_tab_,
        GetUnownedUserDataHost).WillRepeatedly(
            testing::ReturnRef(user_data_host_));

    // Create the feature object using the mock tab.
    my_feature_.emplace(&mock_tab_, ...);
  }

 private:
  // Ensure the destructors get called in the right order so there are no
  // dangling references.
  UnownedUserDataHost user_data_host_;
  MockTabInterface mock_tab_;
  std::optional<MyTabFeature> my_feature_;
```

With this, `mock_tab_` will have a real `MyTabFeature` that can be retrieved via
`MyTabFeature::From(tab)` in the code under test.

You can put any number of features into the mock tab or browser window this way.

## Supporting Injection for Integration Testing

If you want to be able to substitute a mock or test-specific version of your
feature object for testing, go to the line in `browser_window_features.cc` or
`tab_features.cc` that creates your feature object and change it to use
`GetUserDataFactory().CreateInstance()`.

For the example "MyTabFeature" object, it would look like this:

```cpp
  // The type of this data member does not change; it is still a
  // std::unique_ptr<MyTabFeature>.
  my_tab_feature_ = GetUserDataFactory().CreateInstance<MyTabFeature>(
    tab,       // This is for the injection function.
    &tab, ...  // These are the normal constructor args.
  );
```

This creates an object of type `MyTabFeature` _unless_ a factory override has
been installed by a test, in which case it creates whatever the test designated.

## Injecting Features in Integration Tests

A common way to test a feature object is to mock it and inject the mock into a
live browser, to be able to monitor when it gets called an control how it
behaves. In order to do this, your feature object must support injection (see
above). Then in your test setup, before a browser or tab collection is created,
call `GetUserDataFactoryForTesting().AddOverrideForTesting()`.

An example using a test tab feature is shown here:

```cpp
  // In constructor or SetUp():
  my_tab_feature_override_ =
      tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindRepeating([](tabs::TabInterface& tab) {
            // Note that we could set some EXPECT_CALL() here for the mock
            // before returning it. We could also retrieve other features or
            // data from `tab` to help set up the mock.
            return std::make_unique<MyTabFeatureMock>(&tab);
          }));

 private:
  // The override lasts until this object is destroyed.
  ui::UserDataFactory::ScopedOverride my_tab_feature_override_;
```
Now, _every time a tab is created_ it will get a `MyTabFeatureMock`, and anyone
calling `MyTabFeature::From(tab)` will get the mock for that tab.
