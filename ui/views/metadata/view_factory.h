// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_VIEW_FACTORY_H_
#define UI_VIEWS_METADATA_VIEW_FACTORY_H_

#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/macros/concat.h"
#include "base/macros/remove_parens.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_utils.h"
#include "ui/views/views_export.h"

namespace views {

// The builder for `View` inherits from this class by virtue of the declaration
// macro pretending that `View`'s base class is `BaseView`.
template <typename Builder>
class BaseViewBuilderT : public internal::ViewBuilderCore {
 public:
  using ViewClass_ = typename internal::ViewClassTrait<Builder>::ViewClass_;

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
    auto setter =
        std::make_unique<internal::ClassPropertyValueSetter<ViewClass_, T>>(
            property, value);
    internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
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
    auto setter =
        std::make_unique<internal::ClassPropertyMoveSetter<ViewClass_, T>>(
            property, value);
    internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
    return *static_cast<Builder*>(this);
  }

  template <typename T>
  Builder&& SetProperty(const ui::ClassProperty<T*>* property,
                        ui::metadata::ArgType<T> value) && {
    return std::move(this->SetProperty(property, value));
  }

  template <typename T>
  Builder& SetProperty(const ui::ClassProperty<T*>* property, T&& value) & {
    auto setter =
        std::make_unique<internal::ClassPropertyMoveSetter<ViewClass_, T>>(
            property, std::move(value));
    internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
    return *static_cast<Builder*>(this);
  }

  template <typename T>
  Builder&& SetProperty(const ui::ClassProperty<T*>* property, T&& value) && {
    return std::move(this->SetProperty(property, value));
  }

  template <typename T>
  Builder& SetProperty(const ui::ClassProperty<T*>* property,
                       std::unique_ptr<T> value) & {
    auto setter =
        std::make_unique<internal::ClassPropertyUniquePtrSetter<ViewClass_, T>>(
            property, std::move(value));
    internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
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
    std::vector<internal::ViewBuilderCore*> children = {args...};
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

}  // namespace views

// Example of builder classes generated by the following macro usage:
// ```
// BEGIN_VIEW_BUILDER(, XYZView, views::View)
// VIEW_BUILDER_PROPERTY(bool, Enabled)
// VIEW_BUILDER_PROPERTY(bool, Visible)
// END_VIEW_BUILDER
//
// DEFINE_VIEW_BUILDER(, XYZView)
// ```
//
// =>
//
// ```
// template <typename BuilderT>
// class XYZViewBuilderT : public views::ViewBuilderT<BuilderT> {
//  private:
//   using ViewClass_ = XYZView;
//
//  public:
//   XYZViewBuilderT() = default;
//   explicit XYZViewBuilderT(
//       typename ::views::internal::ViewClassTrait<BuilderT>::ViewClass_* view)
//       : views::ViewBuilderT<BuilderT>(view) {}
//   explicit XYZViewBuilderT(
//       std::unique_ptr<
//           typename ::views::internal::ViewClassTrait<BuilderT>::ViewClass_>
//           view)
//       : views::ViewBuilderT<BuilderT>(std::move(view)) {}
//   template <typename OtherBuilder>
//     requires(
//         !std::same_as<BuilderT, OtherBuilder> &&
//         std::convertible_to<
//             typename BaseViewBuilderT<OtherBuilder>::ViewClass_*,
//             ViewClass_*>)
//   view_class##BuilderT(BaseViewBuilderT<OtherBuilder> && other)
//       : ancestor##BuilderT<BuilderT>(std::move(other)) {}
//   ViewBuilderT(ViewBuilderT&&) = default;
//   ViewBuilderT& operator=(ViewBuilderT&&) = default;
//   ~ViewBuilderT() override = default;
//
//   BuilderT& SetEnabled(::ui::metadata::ArgType<bool> value)& {
//     auto setter = std::make_unique<::views::internal::PropertySetter<
//         ViewClass, ::ui::metadata::ArgType<bool>,
//         decltype(&ViewClass_::SetEnabled),
//         &ViewClass_::SetEnabled>>(std::move(value));
//     ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
//     return *static_cast<BuilderT*>(this);
//   }
//
//   BuilderT& SetVisible(::ui::metadata::ArgType<bool> value) {
//     auto setter = std::make_unique<::views::internal::PropertySetter<
//         ViewClass, ::ui::metadata::ArgType<bool>,
//         decltype(&ViewClass_::SetVisible),
//         &ViewClass::SetVisible>>(std::move(value));
//     ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
//     return *static_cast<BuilderT*>(this);
//   }
// };
//
// namespace views {
// template <>
// class Builder<XYZView> : public XYZViewBuilderT<Builder<XYZView>> {
//  private:
//   using ViewClass_ = XYZView;
//
//  public:
//   Builder() = default;
//   explicit Builder(ViewClass_* view) : XYZViewBuilderT<Builder>(view) {}
//   explicit Builder(std::unique_ptr<ViewClass_> view)
//       : XYZViewBuilderT<Builder>(std::move(view)) {}
//   template <typename OtherBuilder>
//     requires(!std::same_as<Builder, OtherBuilder> &&
//              std::convertible_to<
//                  typename BaseViewBuilderT<OtherBuilder>::ViewClass_*,
//                  ViewClass_*>)
//   Builder(BaseViewBuilderT<OtherBuilder>&& other)
//       : view_class##BuilderT<Builder>(std::move(other)) {}
//   Builder(Builder&&) = default;
//   Builder& operator=(Builder&&) = default;
//   ~Builder() = default;
//   [[nodiscard]] std::unique_ptr<internal::ViewBuilderCore> Release()
//       override {
//     return std::make_unique<Builder>(std::move(*this));
//   }
// };
// }  // namespace views
// ```

// The maximum number of overloaded params is currently 3, which is more than
// any callsite uses. Extend these macros if you need more.
#define NUM_ARGS_IMPL(_1, _2, _3, N, ...) N
#define NUM_ARGS(...) NUM_ARGS_IMPL(__VA_ARGS__, 3, 2, 1)

// This will expand the list of types into a parameter declaration list.
// e.g. `DECL_PARAMS(int&, const char*, T&&)` will expand to:
// `int& param3, const char* param2, T&& param1`
#define DECL_PARAM1(type) type param1
#define DECL_PARAM2(type, ...) type param2, DECL_PARAM1(__VA_ARGS__)
#define DECL_PARAM3(type, ...) type param3, DECL_PARAM2(__VA_ARGS__)
#define DECL_PARAMS(...) \
  BASE_CONCAT(DECL_PARAM, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

// This will expand into list of parameters suitable for calling a function
// using the same param names from the above expansion.
// eg: `PASS_PARAMS(int&, const char*, T&&)` will expand to:
// `static_cast<int&>(param3), static_cast<const char*>(param2),
//  static_cast<T&&>(param1)`
// The casts look unnecessary, but actually achieve perfect forwarding: if a
// parameter is declared as `T&&` with `T` being a template type,
// `std::forward<T>()` is equivalent to `static_cast<T&&>()`. In any case where
// the type of the parameter is not an rvalue ref, the cast is a no-op.
#define PASS_PARAM1(type) static_cast<type>(param1)
#define PASS_PARAM2(type, ...) \
  static_cast<type>(param2), PASS_PARAM1(__VA_ARGS__)
#define PASS_PARAM3(type, ...) \
  static_cast<type>(param3), PASS_PARAM2(__VA_ARGS__)
#define PASS_PARAMS(...) \
  BASE_CONCAT(PASS_PARAM, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

// Ensures the supplied args are surrounded by angle brackets.
#define CHECK_TEMPLATE_TYPES_SYNTAX(...) \
  CHECK_TEMPLATE_TYPES_SYNTAX_EXPANDED(__VA_ARGS__)
#define CHECK_TEMPLATE_TYPES_SYNTAX_EXPANDED(...)                              \
  static_assert(                                                               \
      [] {                                                                     \
        constexpr auto a = std::string_view(#__VA_ARGS__);                     \
        return a.length() >= 2 && a.front() == '<' && a.back() == '>';         \
      }(),                                                                     \
      "Template type arg is not surrounded by angle brackets; did you supply " \
      "multiple types without wrapping the whole arg in parens?")

// BEGIN_VIEW_BUILDER, END_VIEW_BUILDER and VIEW_BUILDER_XXXX macros should
// be placed into the same namespace as the 'view_class' parameter.

#define BEGIN_VIEW_BUILDER(export, view_class, ancestor)                      \
  template <typename BuilderT>                                                \
  class export view_class##BuilderT : public ancestor##BuilderT<BuilderT> {   \
   private:                                                                   \
    using ViewClass_ = view_class;                                            \
                                                                              \
   public:                                                                    \
    view_class##BuilderT() = default;                                         \
    explicit view_class##BuilderT(                                            \
        typename ::views::internal::ViewClassTrait<BuilderT>::ViewClass_*     \
            view)                                                             \
        : ancestor##BuilderT<BuilderT>(view) {}                               \
    explicit view_class##BuilderT(                                            \
        std::unique_ptr<                                                      \
            typename ::views::internal::ViewClassTrait<BuilderT>::ViewClass_> \
            view)                                                             \
        : ancestor##BuilderT<BuilderT>(std::move(view)) {}                    \
    template <typename OtherBuilder>                                          \
      requires(                                                               \
          !std::same_as<BuilderT, OtherBuilder> &&                            \
          std::convertible_to<                                                \
              typename ::views::BaseViewBuilderT<OtherBuilder>::ViewClass_*,  \
              ViewClass_*>)                                                   \
    explicit view_class##BuilderT(                                            \
        ::views::BaseViewBuilderT<OtherBuilder>&& other)                      \
        : ancestor##BuilderT<BuilderT>(std::move(other)) {}                   \
    view_class##BuilderT(view_class##BuilderT&&) = default;                   \
    view_class##BuilderT& operator=(view_class##BuilderT&&) = default;        \
    ~view_class##BuilderT() override = default;

#define VIEW_BUILDER_PROPERTY2(property_type, property_name)                  \
  BuilderT& Set##property_name(                                               \
      ::ui::metadata::ArgType<property_type> value)& {                        \
    auto setter = std::make_unique<::views::internal::PropertySetter<         \
        ViewClass_, property_type, decltype(&ViewClass_::Set##property_name), \
        &ViewClass_::Set##property_name>>(std::move(value));                  \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  BuilderT&& Set##property_name(                                              \
      ::ui::metadata::ArgType<property_type> value)&& {                       \
    return std::move(this->Set##property_name(std::move(value)));             \
  }

#define VIEW_BUILDER_PROPERTY3(property_type, property_name, field_type)      \
  BuilderT& Set##property_name(                                               \
      ::ui::metadata::ArgType<property_type> value)& {                        \
    auto setter = std::make_unique<::views::internal::PropertySetter<         \
        ViewClass_, property_type, decltype(&ViewClass_::Set##property_name), \
        &ViewClass_::Set##property_name, field_type>>(std::move(value));      \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  BuilderT&& Set##property_name(                                              \
      ::ui::metadata::ArgType<property_type> value)&& {                       \
    return std::move(this->Set##property_name(std::move(value)));             \
  }

#define VIEW_BUILDER_PROPERTY(...)                                           \
  NUM_ARGS_IMPL(__VA_ARGS__, VIEW_BUILDER_PROPERTY3, VIEW_BUILDER_PROPERTY2) \
  (__VA_ARGS__)

// For use with templated setters. Supply the template type list, in angle
// brackets, in the first arg; if there are multiple template args, wrap in
// parens so the preprocessor keeps everything in one arg. After the property
// name, list the param types. For example:
// ```
//   template <typename T>
//   void SetValue(T value);
//
//   template <typename T, typename U>
//   void SetProperty(const ui::ClassProperty<T>* property, U&& value);
//
// =>
//
//   VIEW_BUILDER_TEMPLATED_PROPERTY(<typename T>, Value, T)
//   VIEW_BUILDER_TEMPLATED_PROPERTY((<typename T, typename U>),
//                                   Property,
//                                   const ui::ClassProperty<T>*,
//                                   U&&)
#define VIEW_BUILDER_TEMPLATED_PROPERTY(template_types, property_name, ...)   \
  CHECK_TEMPLATE_TYPES_SYNTAX(BASE_REMOVE_PARENS(template_types));            \
  template BASE_REMOVE_PARENS(template_types)                                 \
      BuilderT& Set##property_name(DECL_PARAMS(__VA_ARGS__))& {               \
    auto caller = std::make_unique<::views::internal::ClassMethodCaller<      \
        ViewClass_,                                                           \
        decltype(static_cast<void (ViewClass_::*)(__VA_ARGS__)>(              \
            &ViewClass_::Set##property_name)),                                \
        &ViewClass_::Set##property_name, __VA_ARGS__>>(                       \
        PASS_PARAMS(__VA_ARGS__));                                            \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(caller)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  template BASE_REMOVE_PARENS(template_types)                                 \
      BuilderT&& Set##property_name(DECL_PARAMS(__VA_ARGS__))&& {             \
    return std::move(this->Set##property_name(PASS_PARAMS(__VA_ARGS__)));     \
  }

// Sometimes the method being called is on the ancestor to ViewClass_. This
// macro will ensure the overload casts function correctly by specifying the
// ancestor class on which the method is declared. In most cases the following
// macro will be used.
// NOTE: See the Builder declaration for DialogDelegateView in dialog_delegate.h
//       for an example.
#define VIEW_BUILDER_OVERLOAD_METHOD_CLASS(class_name, method_name, ...)      \
  BuilderT& method_name(DECL_PARAMS(__VA_ARGS__))& {                          \
    auto caller = std::make_unique<::views::internal::ClassMethodCaller<      \
        ViewClass_,                                                           \
        decltype(static_cast<void (class_name::*)(__VA_ARGS__)>(              \
            &ViewClass_::method_name)),                                       \
        &class_name::method_name, __VA_ARGS__>>(PASS_PARAMS(__VA_ARGS__));    \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(caller)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  BuilderT&& method_name(DECL_PARAMS(__VA_ARGS__))&& {                        \
    return std::move(this->method_name(PASS_PARAMS(__VA_ARGS__)));            \
  }

// Unless the above scenario is in play, please favor the use of this macro for
// declaring overloaded builder methods.
#define VIEW_BUILDER_OVERLOAD_METHOD(method_name, ...) \
  VIEW_BUILDER_OVERLOAD_METHOD_CLASS(ViewClass_, method_name, __VA_ARGS__)

#define VIEW_BUILDER_METHOD(method_name, ...)                                 \
  template <typename... Args>                                                 \
  BuilderT& method_name(Args&&... args)& {                                    \
    auto caller = std::make_unique<::views::internal::ClassMethodCaller<      \
        ViewClass_, decltype(&ViewClass_::method_name),                       \
        &ViewClass_::method_name, __VA_ARGS__>>(std::forward<Args>(args)...); \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(caller)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  template <typename... Args>                                                 \
  BuilderT&& method_name(Args&&... args)&& {                                  \
    return std::move(this->method_name(std::forward<Args>(args)...));         \
  }

// Enables exposing a template or ambiguously-named method by providing an alias
// that will be used on the Builder.
//
// Examples:
//
//   VIEW_BUILDER_METHOD_ALIAS(
//       AddTab, AddTab<View>, std::string_view, unique_ptr<View>)
//
//   VIEW_BUILDER_METHOD_ALIAS(UnambiguousName, AmbiguousName, int)
//
#define VIEW_BUILDER_METHOD_ALIAS(builder_method, view_method, ...)           \
  template <typename... Args>                                                 \
  BuilderT& builder_method(Args&&... args)& {                                 \
    auto caller = std::make_unique<::views::internal::ClassMethodCaller<      \
        ViewClass_, decltype(&ViewClass_::view_method),                       \
        &ViewClass_::view_method, __VA_ARGS__>>(std::forward<Args>(args)...); \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(caller)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  template <typename... Args>                                                 \
  BuilderT&& builder_method(Args&&... args)&& {                               \
    return std::move(this->builder_method(std::forward<Args>(args)...));      \
  }

#define VIEW_BUILDER_VIEW_TYPE_PROPERTY(property_type, property_name)         \
  template <typename _View>                                                   \
  BuilderT& Set##property_name(_View&& view)& {                               \
    auto setter = std::make_unique<::views::internal::ViewBuilderSetter<      \
        ViewClass_, property_type,                                            \
        decltype(&ViewClass_::Set##property_name<property_type>),             \
        &ViewClass_::Set##property_name<property_type>>>(view.Release());     \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  template <typename _View>                                                   \
  BuilderT&& Set##property_name(_View&& view)&& {                             \
    return std::move(this->Set##property_name(std::move(view)));              \
  }

#define VIEW_BUILDER_VIEW_PROPERTY(property_type, property_name)              \
  template <typename _View>                                                   \
  BuilderT& Set##property_name(_View&& view)& {                               \
    auto setter = std::make_unique<::views::internal::ViewBuilderSetter<      \
        ViewClass_, property_type, decltype(&ViewClass_::Set##property_name), \
        &ViewClass_::Set##property_name>>(view.Release());                    \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter)); \
    return *static_cast<BuilderT*>(this);                                     \
  }                                                                           \
  template <typename _View>                                                   \
  BuilderT&& Set##property_name(_View&& view)&& {                             \
    return std::move(this->Set##property_name(std::move(view)));              \
  }

#define VIEW_BUILDER_PROPERTY_DEFAULT(property_type, property_name, default)   \
  BuilderT& Set##property_name(::ui::metadata::ArgType<property_type> value =  \
                                   default)& {                                 \
    auto setter = std::make_unique<::views::internal::PropertySetter<          \
        ViewClass_, property_type, decltype(&ViewClass_::Set##property_name),  \
        &ViewClass_::Set##property_name>>(std::move(value));                   \
    ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter));  \
    return *static_cast<BuilderT*>(this);                                      \
  }                                                                            \
  BuilderT&& Set##property_name(::ui::metadata::ArgType<property_type> value = \
                                    default)&& {                               \
    return std::move(this->Set##property_name(value));                         \
  }

// clang-format places the semi-colon on a separate line.
// clang-format off
#define END_VIEW_BUILDER };
// clang-format on

// Unlike the above macros, DEFINE_VIEW_BUILDER must be placed in the global
// namespace. Unless 'view_class' is already in the 'views' namespace, it should
// be fully qualified with the namespace in which it lives.
#define DEFINE_VIEW_BUILDER(export, view_class)                              \
  namespace views {                                                          \
  template <>                                                                \
  class export Builder<view_class>                                           \
      : public view_class##BuilderT<Builder<view_class>> {                   \
   private:                                                                  \
    using ViewClass_ = view_class;                                           \
                                                                             \
   public:                                                                   \
    Builder() = default;                                                     \
    explicit Builder(ViewClass_* view)                                       \
        : view_class##BuilderT<Builder>(view) {}                             \
    explicit Builder(std::unique_ptr<ViewClass_> view)                       \
        : view_class##BuilderT<Builder>(std::move(view)) {}                  \
    template <typename OtherBuilder>                                         \
      requires(                                                              \
          !std::same_as<Builder, OtherBuilder> &&                            \
          std::convertible_to<                                               \
              typename ::views::BaseViewBuilderT<OtherBuilder>::ViewClass_*, \
              ViewClass_*>)                                                  \
    explicit Builder(::views::BaseViewBuilderT<OtherBuilder>&& other)        \
        : view_class##BuilderT<Builder>(std::move(other)) {}                 \
    Builder(Builder&&) = default;                                            \
    Builder& operator=(Builder&&) = default;                                 \
    ~Builder() = default;                                                    \
    [[nodiscard]] std::unique_ptr<internal::ViewBuilderCore> Release()       \
        override {                                                           \
      return std::make_unique<Builder>(std::move(*this));                    \
    }                                                                        \
  };                                                                         \
  }  // namespace views

#endif  // UI_VIEWS_METADATA_VIEW_FACTORY_H_
