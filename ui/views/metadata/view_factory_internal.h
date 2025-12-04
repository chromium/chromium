// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_
#define UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_

#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views::internal {

template <typename T>
class Builder {};

template <typename T>
class ViewClassTrait {};

template <typename T>
class ViewClassTrait<Builder<T>> {
 public:
  using ViewClass_ = T;
};

class PropertySetterBase {
 public:
  PropertySetterBase() = default;
  PropertySetterBase(const PropertySetterBase&) = delete;
  PropertySetterBase& operator=(const PropertySetterBase&) = delete;
  virtual ~PropertySetterBase() = default;

  virtual void SetProperty(View* obj) = 0;
};

template <typename TClass,
          typename TValue,
          typename TSig,
          TSig Set,
          typename FType = typename std::remove_reference<TValue>::type>
class PropertySetter : public PropertySetterBase {
 public:
  explicit PropertySetter(ui::metadata::ArgType<TValue> value)
      : value_(std::move(value)) {}
  PropertySetter(const PropertySetter&) = delete;
  PropertySetter& operator=(const PropertySetter&) = delete;
  ~PropertySetter() override = default;

  void SetProperty(View* obj) override {
    (static_cast<TClass*>(obj)->*Set)(std::move(value_));
  }

 private:
  FType value_;
};

template <typename TClass, typename TValue>
class ClassPropertyValueSetter : public PropertySetterBase {
 public:
  ClassPropertyValueSetter(const ui::ClassProperty<TValue>* property,
                           TValue value)
      : property_(property), value_(value) {}
  ClassPropertyValueSetter(const ClassPropertyValueSetter&) = delete;
  ClassPropertyValueSetter& operator=(const ClassPropertyValueSetter&) = delete;
  ~ClassPropertyValueSetter() override = default;

  void SetProperty(View* obj) override {
    static_cast<TClass*>(obj)->SetProperty(property_, value_);
  }

 private:
  // This field is not a raw_ptr<> because of compiler error when passed to
  // templated param T*.
  RAW_PTR_EXCLUSION const ui::ClassProperty<TValue>* property_;
  TValue value_;
};

template <typename TClass, typename TValue>
class ClassPropertyMoveSetter : public PropertySetterBase {
 public:
  template <typename U>
    requires(std::constructible_from<TValue, U &&>)
  ClassPropertyMoveSetter(const ui::ClassProperty<TValue*>* property, U&& value)
      : property_(property), value_(std::forward<U>(value)) {}
  ClassPropertyMoveSetter(const ClassPropertyMoveSetter&) = delete;
  ClassPropertyMoveSetter& operator=(const ClassPropertyMoveSetter&) = delete;
  ~ClassPropertyMoveSetter() override = default;

  void SetProperty(View* obj) override {
    static_cast<TClass*>(obj)->SetProperty(property_.get(), std::move(value_));
  }

 private:
  raw_ptr<const ui::ClassProperty<TValue*>> property_;
  TValue value_;
};

template <typename TClass, typename TValue>
class ClassPropertyUniquePtrSetter : public PropertySetterBase {
 public:
  ClassPropertyUniquePtrSetter(const ui::ClassProperty<TValue*>* property,
                               std::unique_ptr<TValue> value)
      : property_(property), value_(std::move(value)) {}
  ClassPropertyUniquePtrSetter(const ClassPropertyUniquePtrSetter&) = delete;
  ClassPropertyUniquePtrSetter& operator=(const ClassPropertyUniquePtrSetter&) =
      delete;
  ~ClassPropertyUniquePtrSetter() override = default;

  void SetProperty(View* obj) override {
    static_cast<TClass*>(obj)->SetProperty(property_, std::move(value_));
  }

 private:
  raw_ptr<const ui::ClassProperty<TValue*>> property_;
  std::unique_ptr<TValue> value_;
};

template <typename TClass, typename TSig, TSig Set, typename... Args>
class ClassMethodCaller : public PropertySetterBase {
 public:
  explicit ClassMethodCaller(Args... args)
      : args_(std::make_tuple<Args...>(std::move(args)...)) {}
  ClassMethodCaller(const ClassMethodCaller&) = delete;
  ClassMethodCaller& operator=(const ClassMethodCaller&) = delete;
  ~ClassMethodCaller() override = default;

  void SetProperty(View* obj) override {
    std::apply(Set, std::tuple_cat(std::make_tuple(static_cast<TClass*>(obj)),
                                   std::move(args_)));
  }

 private:
  using Parameters = std::tuple<typename std::remove_reference<Args>::type...>;
  Parameters args_;
};

class VIEWS_EXPORT ViewBuilderCore {
 public:
  ViewBuilderCore();
  ViewBuilderCore(ViewBuilderCore&&);
  ViewBuilderCore& operator=(ViewBuilderCore&&);
  virtual ~ViewBuilderCore();

  [[nodiscard]] std::unique_ptr<View> Build() &&;
  [[nodiscard]] virtual std::unique_ptr<ViewBuilderCore> Release() = 0;

 protected:
  // Vector of child view builders. If the optional index is included it will be
  // passed to View::AddChildViewAt().
  using ChildList = std::vector<
      std::pair<std::unique_ptr<ViewBuilderCore>, std::optional<size_t>>>;
  using PropertyList = std::vector<std::unique_ptr<PropertySetterBase>>;

  void AddPropertySetter(std::unique_ptr<PropertySetterBase> setter);
  void CreateChildren(View* parent);
  virtual std::unique_ptr<View> DoBuild() = 0;
  void SetProperties(View* view);

  ChildList children_;
  PropertyList property_list_;
};

template <typename TClass, typename TValue, typename TSig, TSig Set>
class ViewBuilderSetter : public PropertySetterBase {
 public:
  explicit ViewBuilderSetter(std::unique_ptr<ViewBuilderCore> builder)
      : builder_(std::move(builder)) {}
  ViewBuilderSetter(const ViewBuilderSetter&) = delete;
  ViewBuilderSetter& operator=(const ViewBuilderSetter&) = delete;
  ~ViewBuilderSetter() override = default;

  void SetProperty(View* obj) override {
    (static_cast<TClass*>(obj)->*Set)(std::move(*builder_).Build());
  }

 private:
  std::unique_ptr<ViewBuilderCore> builder_;
};

// The builder for `View` inherits from this class by virtue of the declaration
// macro pretending that `View`'s base class is `BaseView`.
template <typename Builder>
class BaseViewBuilderT : public ViewBuilderCore {
 public:
  using ViewClass_ = typename ViewClassTrait<Builder>::ViewClass_;

 private:
  using OwnedPtr = std::unique_ptr<ViewClass_>;
  using Ptr = raw_ptr<ViewClass_>;
  using ViewStorage = std::variant<OwnedPtr, Ptr>;

 public:
  explicit BaseViewBuilderT(OwnedPtr view = std::make_unique<ViewClass_>())
      : view_(std::move(view)) {
    CHECK(std::get<OwnedPtr>(view_));
  }
  explicit BaseViewBuilderT(ViewClass_* view) : view_(view) {
    CHECK(std::get<Ptr>(view_));
  }

  // Implicit conversion from Builder<Derived> to Builder<Base>.
  template <typename OtherBuilder>
    requires(!std::same_as<Builder, OtherBuilder> &&
             std::convertible_to<
                 typename BaseViewBuilderT<OtherBuilder>::ViewClass_*,
                 ViewClass_*>)
  BaseViewBuilderT(BaseViewBuilderT<OtherBuilder>&& other)
      : view_(
            std::visit([](auto&& view) { return ViewStorage(std::move(view)); },
                       std::move(other.view_))),
        configure_callbacks_(
            ConvertCallbacks(std::move(other.configure_callbacks_))),
        after_build_callbacks_(
            ConvertCallbacks(std::move(other.after_build_callbacks_))) {}

  // Move construction/assignment. (Copy is not possible, no copyable members.)
  BaseViewBuilderT(BaseViewBuilderT&&) = default;
  BaseViewBuilderT& operator=(BaseViewBuilderT&&) = default;

  ~BaseViewBuilderT() override = default;

  template <typename Child>
  Builder& AddChild(Child&& child) & {
    children_.emplace_back(std::make_pair(child.Release(), std::nullopt));
    return *static_cast<Builder*>(this);
  }

  template <typename Child>
  Builder&& AddChild(Child&& child) && {
    return std::move(this->AddChild(std::move(child)));
  }

  template <typename Child>
  Builder& AddChildAt(Child&& child, size_t index) & {
    children_.emplace_back(std::make_pair(child.Release(), index));
    return *static_cast<Builder*>(this);
  }

  template <typename Child>
  Builder&& AddChildAt(Child&& child, size_t index) && {
    return std::move(this->AddChildAt(std::move(child), index));
  }

  template <typename Child, typename... Types>
  Builder& AddChildren(Child&& child, Types&&... args) & {
    return AddChildrenImpl(&child, &args...);
  }

  template <typename Child, typename... Types>
  Builder&& AddChildren(Child&& child, Types&&... args) && {
    return std::move(this->AddChildrenImpl(&child, &args...));
  }

  template <typename T>
  Builder& SetProperty(const ui::ClassProperty<T>* property,
                       ui::metadata::ArgType<T> value) & {
    auto setter = std::make_unique<ClassPropertyValueSetter<ViewClass_, T>>(
        property, value);
    ViewBuilderCore::AddPropertySetter(std::move(setter));
    return *static_cast<Builder*>(this);
  }

  template <typename T>
  Builder&& SetProperty(const ui::ClassProperty<T>* property,
                        ui::metadata::ArgType<T> value) && {
    return std::move(this->SetProperty(property, value));
  }

  template <typename T>
  Builder& SetProperty(const ui::ClassProperty<T*>* property,
                       ui::metadata::ArgType<T> value) & {
    auto setter = std::make_unique<ClassPropertyMoveSetter<ViewClass_, T>>(
        property, value);
    ViewBuilderCore::AddPropertySetter(std::move(setter));
    return *static_cast<Builder*>(this);
  }

  template <typename T>
  Builder&& SetProperty(const ui::ClassProperty<T*>* property,
                        ui::metadata::ArgType<T> value) && {
    return std::move(this->SetProperty(property, value));
  }

  template <typename T>
  Builder& SetProperty(const ui::ClassProperty<T*>* property, T&& value) & {
    auto setter = std::make_unique<ClassPropertyMoveSetter<ViewClass_, T>>(
        property, std::move(value));
    ViewBuilderCore::AddPropertySetter(std::move(setter));
    return *static_cast<Builder*>(this);
  }

  template <typename T>
  Builder&& SetProperty(const ui::ClassProperty<T*>* property, T&& value) && {
    return std::move(this->SetProperty(property, value));
  }

  template <typename T>
  Builder& SetProperty(const ui::ClassProperty<T*>* property,
                       std::unique_ptr<T> value) & {
    auto setter = std::make_unique<ClassPropertyUniquePtrSetter<ViewClass_, T>>(
        property, std::move(value));
    ViewBuilderCore::AddPropertySetter(std::move(setter));
    return *static_cast<Builder*>(this);
  }

  template <typename T>
  Builder&& SetProperty(const ui::ClassProperty<T*>* property,
                        std::unique_ptr<T> value) && {
    return std::move(this->SetProperty(property, std::move(value)));
  }

  template <typename ViewPtr>
  Builder& CopyAddressTo(ViewPtr* view_address) & {
    *view_address = std::visit([](auto& view) { return view.get(); }, view_);
    return *static_cast<Builder*>(this);
  }

  template <typename ViewPtr>
  Builder&& CopyAddressTo(ViewPtr* view_address) && {
    return std::move(this->CopyAddressTo(view_address));
  }

  // Schedules `configure_callback` to run after the View is constructed and
  // properties have been set. Calling this repeatedly will result in running
  // all callbacks in the order provided.
  //
  // The difference between this and `AfterBuild()` is that this runs after
  // properties are set but before children are built, while that runs after
  // both.
  template <typename T>
    requires(std::convertible_to<ViewClass_*, T*>)
  Builder& CustomConfigure(base::OnceCallback<void(T*)> configure_callback) & {
    return AddCallbackImpl(std::move(configure_callback), configure_callbacks_);
  }

  template <typename T>
    requires(std::convertible_to<ViewClass_*, T*>)
  Builder&& CustomConfigure(
      base::OnceCallback<void(T*)> configure_callback) && {
    return std::move(this->CustomConfigure(std::move(configure_callback)));
  }

  // Schedules `after_build_callback` to run after View and its children have
  // been constructed. Calling this repeatedly will result in running all
  // callbacks in the order provided.
  //
  // The difference between this and `CustomConfigure()` is that this runs after
  // children are built, while that runs before.
  template <typename T>
    requires(std::convertible_to<ViewClass_*, T*>)
  Builder& AfterBuild(base::OnceCallback<void(T*)> after_build_callback) & {
    return AddCallbackImpl(std::move(after_build_callback),
                           after_build_callbacks_);
  }

  template <typename T>
    requires(std::convertible_to<ViewClass_*, T*>)
  Builder&& AfterBuild(base::OnceCallback<void(T*)> after_build_callback) && {
    return std::move(this->AfterBuild(std::move(after_build_callback)));
  }

  [[nodiscard]] OwnedPtr Build() && {
    CHECK(std::holds_alternative<OwnedPtr>(view_))
        << "Use `BuildChildren()` on `Builder`s of non-owned `View`s.";
    auto view = std::get<OwnedPtr>(std::move(view_));
    SetProperties(view.get());
    DoCustomConfigure(view.get());
    CreateChildren(view.get());
    DoAfterBuild(view.get());
    return view;
  }

  void BuildChildren() && {
    CHECK(std::holds_alternative<Ptr>(view_))
        << "Use `Build()` on `Builder`s of owned `View`s.";
    auto view = std::get<Ptr>(view_);
    SetProperties(view);
    DoCustomConfigure(view);
    CreateChildren(view);
    DoAfterBuild(view);
  }

 private:
  // Allow conversion to other builders when appropriate. This friend
  // declaration is necessary since `BaseViewBuilderT<T>` and
  // `BaseViewBuilderT<U>` are unrelated types and normally cannot access
  // non-public members of each other.
  template <typename T>
  friend class BaseViewBuilderT;

  using ConfigureCallback = base::OnceCallback<void(ViewClass_*)>;

  // Thunks callbacks in `other_callbacks` to be invocable from this object.
  // This is intended only for use by the "implicit upcast" constructor; see
  // comments in implementation.
  template <typename T>
    requires(!std::same_as<ViewClass_, T> &&
             std::convertible_to<T*, ViewClass_*>)
  static std::vector<ConfigureCallback> ConvertCallbacks(
      std::vector<base::OnceCallback<void(T*)>> other_callbacks) {
    std::vector<ConfigureCallback> callbacks;
    callbacks.reserve(other_callbacks.size());
    for (auto& callback : other_callbacks) {
      callbacks.push_back(base::BindOnce(
          [](base::OnceCallback<void(T*)> cb, ViewClass_* view) {
            // This downcast should always succeed, since the source view was
            // originally created on a `Builder<T>` (or subclass-of-`T`).
            auto* const t = AsViewClass<T>(view);
            CHECK(t);
            std::move(cb).Run(t);
          },
          std::move(callback)));
    }
    return callbacks;
  }

  // Internal implementation which iterates over all the parameters without
  // resorting to recursion which can lead to more code generation.
  template <typename... Args>
  Builder& AddChildrenImpl(Args*... args) & {
    std::vector<ViewBuilderCore*> children = {args...};
    for (auto* child : children) {
      children_.emplace_back(std::make_pair(child->Release(), std::nullopt));
    }
    return *static_cast<Builder*>(this);
  }

  template <typename T>
    requires(std::convertible_to<ViewClass_*, T*>)
  Builder& AddCallbackImpl(base::OnceCallback<void(T*)> callback,
                           std::vector<ConfigureCallback>& callbacks) & {
    if constexpr (std::same_as<T, ViewClass_>) {
      callbacks.push_back(std::move(callback));
    } else {
      callbacks.push_back(
          base::BindOnce([](base::OnceCallback<void(T*)> cb,
                            ViewClass_* view) { std::move(cb).Run(view); },
                         std::move(callback)));
    }
    return *static_cast<Builder*>(this);
  }

  void DoCustomConfigure(ViewClass_* view) {
    for (auto& cb : configure_callbacks_) {
      std::move(cb).Run(view);
    }
  }

  std::unique_ptr<View> DoBuild() override { return std::move(*this).Build(); }

  void DoAfterBuild(ViewClass_* view) {
    for (auto& cb : after_build_callbacks_) {
      std::move(cb).Run(view);
    }
  }

  // Controlled view, which may or may not be owned by this builder.
  ViewStorage view_;

  // Optional callbacks invoked right before `CreateChildren()`.
  std::vector<ConfigureCallback> configure_callbacks_;

  // Optional callbacks invoked right after `CreateChildren()`.
  std::vector<ConfigureCallback> after_build_callbacks_;
};

}  // namespace views::internal

#endif  // UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_
