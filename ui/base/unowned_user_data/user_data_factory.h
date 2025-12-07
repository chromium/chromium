// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UNOWNED_USER_DATA_USER_DATA_FACTORY_H_
#define UI_BASE_UNOWNED_USER_DATA_USER_DATA_FACTORY_H_

#include <concepts>
#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/typed_identifier.h"

namespace ui {

// -----------------
//  UserDataFactory
// -----------------
//
// Provides a way for "features" objects (BrowserWindowFeatures, TabFeatures,
// etc.) to create their owned subsystems while also allowing those subsystems
// to be selectively injected in tests.
//
// For example, say that there is a TabWrangler (which must contain a
// `ScopedUnownedUserData`) which will be owned by TabFeatures, and there is a
// subclass TabWranglerImpl which will be used in production and a subclass
// MockTabWrangler which will be used for a specific test.
//
// Here's what TabWrangler might look like:
// ```
//  class TabWrangler {
//   public:
//    TabWrangler(TabInterface& tab) : scoped_data_(tab, *this) { ... }
//
//    // This is always a nice-to-have, but not required:
//    static TabWrangler* From(TabInterface* tab) {
//      return Get(tab->GetUnownedUserDataHost());
//    }
//
//    // Rest of the interface goes here.
//    virtual .....;
//
//   private:
//    ScopedUnownedUserData<TabWrangler> scoped_data_;
//  };
// ```
//
// Then in TabFeatures::Init(), you can do:
// ```
//   tab_wrangler_ =
//       GetUserDataFactory().CreateInstance<TabWranglerImpl>(
//           tab,
//           first_arg_to_wrangler_impl_constructor,
//           second_arg_to_wrangler_impl_constructor,
//           ...);
// ```
//
// This either constructs a TabWranglerImpl from the args provided, or, if a
// test override was previously set, that override is called to create the
// TabWrangler instead. Either way, a valid `std::unique_ptr<TabWrangler>` is
// returned.
//
// To support this functionality, you will need a static method that returns an
// `UserDataFactoryWithOwner<Owner>&`, where `Owner` is your features or
// model object. The `Owner` is provided as an argument to override factory
// methods because they might need to refer to existing concrete (or injected)
// data that has already been created (not all of which may be in an
// `UnownedUserDataHost`).
//
// The factory accessor method needs to be static since you will want to set any
// overrides before actual features objects and models are created. You may want
// to make it non-public to avoid production code outside the class from
// accessing it. Finally, avoid at-exit cleanup, use a
// `static base::NoDestructor<>`.
//
// Here's one possible implementation for TabFeatures:
// ```
//  class TabFeatures {
//    // ...
//   private:
//    static UserDataFactoryWithOwner<TabInterface>& GetUserDataFactory();
//  };
//
//  // static
//  UserDataFactoryWithOwner<TabInterface>& TabFeatures::GetUserDataFactory() {
//    static base::NoDestructor<UserDataFactoryWithOwner<TabInterface>> factory;
//    return *factory;
//  }
// ```
//
// For your tests, ensure that the test can access the factory and call
// `AddOverrideForTesting()` to inject a different type of object or use
// different construction logic.
//
// Here's an example:
// ```
//  class MyTest : public InProcessBrowserTest {
//   public:
//
//    MyTest() {
//      // For the duration of this test, all TabFeatures will use a
//      // `MockTabWrangler` instead of creating a `TabWranglerImpl`.
//      tab_wrangler_override_ =
//          TabFeatures::GetUserDataFactoryForTesting()
//              .AddOverrideForTesting(base::BindRepeating(
//                  [](TabInterface& tab) {
//                    return std::make_unique<MockTabWrangler>(tab);
//                  }));
//    }
//
//   private:
//    UserDataFactory::ScopedOverride tab_wrangler_override_;
//  };
// ```

class UnownedUserDataHost;
template <typename Owner>
class UserDataFactoryWithOwner;

// Base class for all factories. See `UserDataFactoryWithOwner` for
// public API.
class COMPONENT_EXPORT(UNOWNED_USER_DATA) UserDataFactory {
 public:
  UserDataFactory();
  UserDataFactory(const UserDataFactory&) = delete;
  void operator=(const UserDataFactory&) = delete;
  virtual ~UserDataFactory();

  template <typename T>
  using Key = ui::TypedIdentifier<T>;
  using UntypedKey = ui::ElementIdentifier;

  // Object that is held while the factory method for a specific type is
  // overridden.
  class [[nodiscard]] COMPONENT_EXPORT(UNOWNED_USER_DATA) ScopedOverride {
   public:
    ScopedOverride();
    ScopedOverride(ScopedOverride&&) noexcept;
    ScopedOverride& operator=(ScopedOverride&&) noexcept;
    ~ScopedOverride();

   private:
    template <typename Owner>
    friend class UserDataFactoryWithOwner;
    ScopedOverride(UserDataFactory& factory, UntypedKey key);
    void Release();

    raw_ptr<UserDataFactory> factory_ = nullptr;
    UntypedKey key_;
  };

 protected:
  struct Entry {
    virtual ~Entry() = default;
  };

  template <typename FactoryMethod>
  struct TypedEntry : public Entry {
    explicit TypedEntry(FactoryMethod factory_method_)
        : factory_method(factory_method_) {}
    ~TypedEntry() override = default;
    FactoryMethod factory_method;
  };

  template <typename FactoryMethod, typename T>
  TypedEntry<FactoryMethod>* GetEntry(Key<T> key) {
    const auto entry = entries_.find(key.identifier());
    return entry != entries_.end()
               ? static_cast<TypedEntry<FactoryMethod>*>(entry->second.get())
               : nullptr;
  }

  void AddEntry(UntypedKey key, std::unique_ptr<Entry> entry);

 private:
  std::map<UntypedKey, std::unique_ptr<Entry>> entries_;
};

// Factory where overrides take a specific `Owner` object. What you choose to be
// the owner depends on what data the injected objects will need to access when
// they are created; in most cases it's the model or features object.
template <typename Owner>
class UserDataFactoryWithOwner : public UserDataFactory {
 public:
  UserDataFactoryWithOwner() = default;
  ~UserDataFactoryWithOwner() override = default;

  // Factory method used to override creation of a unowned user data type
  // specified by `T::kDataKey`.
  template <typename T>
  using TestingOverrideFactoryMethod =
      base::RepeatingCallback<std::unique_ptr<T>(Owner&)>;

  // Creates an unowned data object instance, either by directly constructing
  // it the arguments passed or by referring to a testing override already
  // registered.
  //
  // Will call (if present)
  //   `override.Run(owner_for_override)`
  // or else
  //   `ConcreteType(concrete_type_constructor_args...)`
  // to create an instance of `BaseType`.
  //
  // Note that `owner_for_override` is only used for overrides and not passed to
  // the constructor of `ConcreteType`, so you may find yourself passing the
  // object (or part of it) twice - once for the override, and again for the
  // concrete type's constructor.
  template <typename ConcreteType,
            typename... Args,
            typename BaseType = typename decltype(ConcreteType::kDataKey)::Type>
    requires std::derived_from<ConcreteType, BaseType>
  std::unique_ptr<BaseType> CreateInstance(
      Owner& owner_for_override,
      Args&&... concrete_type_constructor_args) {
    if (auto* const entry = GetEntry<TestingOverrideFactoryMethod<BaseType>>(
            ConcreteType::kDataKey)) {
      return entry->factory_method.Run(owner_for_override);
    }
    return std::make_unique<ConcreteType>(
        std::forward<Args>(concrete_type_constructor_args)...);
  }

  // Creates an unowned data object instance, either by calling `factory_method`
  // with `factory_method_args`, or by referring to a testing override already
  // registered.
  //
  // Will call (if present)
  //   `override.Run(owner_for_override)`
  // or else
  //   `(*factory_method)(factory_method_args...)`
  // to create an instance of `BaseType`.
  //
  // Note that `owner_for_override` is only used for overrides and not passed to
  // `factory_method`, so you may find yourself passing the object (or part of
  // it) twice - once for the override, and again for the factory method.
  template <typename ConcreteType,
            typename... Args,
            typename BaseType = typename decltype(ConcreteType::kDataKey)::Type>
    requires std::derived_from<ConcreteType, BaseType>
  std::unique_ptr<BaseType> CreateInstanceWithFactoryMethod(
      Owner& owner_for_override,
      std::unique_ptr<ConcreteType> (*factory_method)(Args...),
      Args... factory_method_args) {
    if (auto* const entry = GetEntry<TestingOverrideFactoryMethod<BaseType>>(
            ConcreteType::kDataKey)) {
      return entry->factory_method.Run(owner_for_override);
    }
    return (*factory_method)(std::forward<Args>(factory_method_args)...);
  }

  // Override how all unowned user data objects of type `BaseType` will be
  // created when this factory is used. The `factory_method` will be used to
  // create objects of `OverrideType` instead.
  //
  // The override persists as long as the returned value is in scope, when it is
  // destroyed the override is removed.
  template <typename OverrideType,
            typename BaseType = typename decltype(OverrideType::kDataKey)::Type>
    requires std::derived_from<OverrideType, BaseType> &&
             std::same_as<BaseType,
                          typename decltype(OverrideType::kDataKey)::Type>
  ScopedOverride AddOverrideForTesting(
      TestingOverrideFactoryMethod<OverrideType> factory_method) {
    const auto id = BaseType::kDataKey;
    using EntryType = TypedEntry<TestingOverrideFactoryMethod<BaseType>>;
    AddEntry(id.identifier(),
             std::make_unique<EntryType>(factory_method.Then(
                 base::BindRepeating([](std::unique_ptr<BaseType> result) {
                   return std::unique_ptr<BaseType>(std::move(result));
                 }))));
    return ScopedOverride(*this, id.identifier());
  }
};

}  // namespace ui

#endif  // UI_BASE_UNOWNED_USER_DATA_USER_DATA_FACTORY_H_
