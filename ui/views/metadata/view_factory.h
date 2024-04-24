// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_VIEW_FACTORY_H_
#define UI_VIEWS_METADATA_VIEW_FACTORY_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/macros/concat.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/views_export.h"

namespace views {

template <typename Builder>
class BaseViewBuilderT : public internal::ViewBuilderCore {
 public:
  using ViewClass_ = typename internal::ViewClassTrait<Builder>::ViewClass_;
  using AfterBuildCallback = base::OnceCallback<void(ViewClass_*)>;
  using ConfigureCallback = base::OnceCallback<void(ViewClass_*)>;
  BaseViewBuilderT() { view_ = std::make_unique<ViewClass_>(); }
  explicit BaseViewBuilderT(std::unique_ptr<ViewClass_> view) {
    view_ = std::move(view);
  }
  explicit BaseViewBuilderT(ViewClass_* root_view) : root_view_(root_view) {}
  BaseViewBuilderT(BaseViewBuilderT&&) = default;
  BaseViewBuilderT& operator=(BaseViewBuilderT&&) = default;
  ~BaseViewBuilderT() override = default;

  // Schedule `after_build_callback` to run after View and its children have
  // been constructed. Calling this multiple times will chain the callbacks.
  Builder& AfterBuild(AfterBuildCallback after_build_callback) & {
    // Allow multiple after build callbacks by chaining them.
    if (after_build_callback_) {
      after_build_callback_ = base::BindOnce(
          [](AfterBuildCallback previous_callback,
             AfterBuildCallback current_callback, ViewClass_* root_view) {
            std::move(previous_callback).Run(root_view);
            std::move(current_callback).Run(root_view);
          },
          std::move(after_build_callback_), std::move(after_build_callback));
    } else {
      after_build_callback_ = std::move(after_build_callback);
    }
    return *static_cast<Builder*>(this);
  }

  Builder&& AfterBuild(AfterBuildCallback after_build_callback) && {
    return std::move(this->AfterBuild(std::move(after_build_callback)));
  }

  template <typename ViewPtr>
  Builder& CopyAddressTo(ViewPtr* view_address) & {
    *view_address = view_ ? view_.get() : root_view_.get();
    return *static_cast<Builder*>(this);
  }

  template <typename ViewPtr>
  Builder&& CopyAddressTo(ViewPtr* view_address) && {
    return std::move(this->CopyAddressTo(view_address));
  }

  // Schedule `configure_callback` to run after the View is constructed and
  // properties have been set. Calling this multiple times will chain the
  // callbacks.
  Builder& CustomConfigure(ConfigureCallback configure_callback) & {
    // Allow multiple configure callbacks by chaining them.
    if (configure_callback_) {
      configure_callback_ = base::BindOnce(
          [](ConfigureCallback current_callback,
             ConfigureCallback previous_callback, ViewClass_* root_view) {
            std::move(current_callback).Run(root_view);
            std::move(previous_callback).Run(root_view);
          },
          std::move(configure_callback), std::move(configure_callback_));
    } else {
      configure_callback_ = std::move(configure_callback);
    }
    return *static_cast<Builder*>(this);
  }

  Builder&& CustomConfigure(ConfigureCallback configure_callback) && {
    return std::move(this->CustomConfigure(std::move(configure_callback)));
  }

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

  [[nodiscard]] std::unique_ptr<ViewClass_> Build() && {
    DCHECK(!root_view_) << "Root view specified. Use BuildChildren() instead.";
    DCHECK(view_);
    SetProperties(view_.get());
    DoCustomConfigure(view_.get());
    CreateChildren(view_.get());
    DoAfterBuild(view_.get());
    return std::move(view_);
  }

  void BuildChildren() && {
    DCHECK(!view_) << "Default constructor called. Use Build() instead.";
    DCHECK(root_view_);
    SetProperties(root_view_);
    DoCustomConfigure(root_view_);
    CreateChildren(root_view_);
    DoAfterBuild(root_view_);
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

 protected:
  // Internal implementation which iterates over all the parameters without
  // resorting to recursion which can lead to more code generation.
  template <typename... Args>
  Builder& AddChildrenImpl(Args*... args) & {
    std::vector<internal::ViewBuilderCore*> children = {args...};
    for (auto* child : children)
      children_.emplace_back(std::make_pair(child->Release(), std::nullopt));
    return *static_cast<Builder*>(this);
  }

  void DoAfterBuild(ViewClass_* view) {
    if (after_build_callback_) {
      std::move(after_build_callback_).Run(view);
    }
  }

  void DoCustomConfigure(ViewClass_* view) {
    if (configure_callback_)
      std::move(configure_callback_).Run(view);
  }

  std::unique_ptr<View> DoBuild() override { return std::move(*this).Build(); }

  // Optional callback invoked right after calling `CreateChildren()`. This
  // allows additional configuration of the view not easily covered by the
  // builder after all addresses have been copied, properties have been set,
  // and children have themselves been built and added.
  AfterBuildCallback after_build_callback_;

  // Optional callback invoked right before calling CreateChildren. This allows
  // any additional configuration of the view not easily covered by the builder.
  ConfigureCallback configure_callback_;

  // Owned and meaningful during the Builder building process. Its
  // ownership will be transferred out upon Build() call.
  std::unique_ptr<ViewClass_> view_;

  // Unowned root view. Used for creating a builder with an existing root
  // instance.
  raw_ptr<ViewClass_> root_view_ = nullptr;
};

}  // namespace views

// Example of builder class generated by the following macros.
//
// template <typename Builder, typename ViewClass>
// class ViewBuilderT : public BaseViewBuilderT<Builder, ViewClass> {
//  public:
//   ViewBuilderT() = default;
//   ViewBuilderT(const ViewBuilderT&&) = default;
//   ViewBuilderT& operator=(const ViewBuilderT&&) = default;
//   ~ViewBuilderT() override = default;
//
//   Builder& SetEnabled(bool value) {
//     auto setter = std::make_unique<
//         PropertySetter<ViewClass, bool, decltype(&ViewClass::SetEnabled),
//         &ViewClass::SetEnabled>>(value);
//     ViewBuilderCore::AddPropertySetter(std::move(setter));
//     return *static_cast<Builder*>(this);
//   }
//
//   Builder& SetVisible(bool value) {
//     auto setter = std::make_unique<
//         PropertySetter<ViewClass, bool, &ViewClass::SetVisible>>(value);
//     ViewBuilderCore::AddPropertySetter(std::move(setter));
//     return *static_cast<Builder*>(this);
//   }
// };
//
// class VIEWS_EXPORT ViewBuilderTest
//     : public ViewBuilderT<ViewBuilderTest, View> {};
//
// template <typename Builder, typename ViewClass>
// class LabelButtonBuilderT : public ViewBuilderT<Builder, ViewClass> {
//  public:
//   LabelButtonBuilderT() = default;
//   LabelButtonBuilderT(LabelButtonBuilderT&&) = default;
//   LabelButtonBuilderT& operator=(LabelButtonBuilderT&&) = default;
//   ~LabelButtonBuilderT() override = default;
//
//   Builder& SetIsDefault(bool value) {
//     auto setter = std::make_unique<
//         PropertySetter<ViewClass, bool, decltype(&ViewClass::SetIsDefault),
//         &ViewClass::SetIsDefault>>(value);
//     ViewBuilderCore::AddPropertySetter(std::move(setter));
//     return *static_cast<Builder*>(this);
//   }
// };
//
// class VIEWS_EXPORT LabelButtonBuilder
//     : public LabelButtonBuilderT<LabelButtonBuilder, LabelButton> {};

// The maximum number of overloaded params is 10. This should be overkill since
// a function with 10 params is well into the "suspect" territory anyway.
// TODO(kylixrd@): Evaluate whether a max of 5 may be more reasonable.
#define NUM_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define NUM_ARGS(...) NUM_ARGS_IMPL(__VA_ARGS__, _10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

// This will expand the list of types into a parameter declaration list.
// eg: DECL_PARAMS(int, char, float, double) will expand to:
// int param4, char param3, float param2, double param1
#define DECL_PARAM1(type) type param1
#define DECL_PARAM2(type, ...) type param2, DECL_PARAM1(__VA_ARGS__)
#define DECL_PARAM3(type, ...) type param3, DECL_PARAM2(__VA_ARGS__)
#define DECL_PARAM4(type, ...) type param4, DECL_PARAM3(__VA_ARGS__)
#define DECL_PARAM5(type, ...) type param5, DECL_PARAM4(__VA_ARGS__)
#define DECL_PARAM6(type, ...) type param6, DECL_PARAM5(__VA_ARGS__)
#define DECL_PARAM7(type, ...) type param7, DECL_PARAM6(__VA_ARGS__)
#define DECL_PARAM8(type, ...) type param8, DECL_PARAM7(__VA_ARGS__)
#define DECL_PARAM9(type, ...) type param9, DECL_PARAM8(__VA_ARGS__)
#define DECL_PARAM10(type, ...) type param10, DECL_PARAM9(__VA_ARGS__)
#define DECL_PARAMS(...) \
  BASE_CONCAT(DECL_PARAM, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

// This will expand into list of parameters suitable for calling a function
// using the same param names from the above expansion.
// eg: PASS_PARAMS(int, char, float, double)
// param4, param3, param2, param1
#define PASS_PARAM1(type) param1
#define PASS_PARAM2(type, ...) param2, PASS_PARAM1(__VA_ARGS__)
#define PASS_PARAM3(type, ...) param3, PASS_PARAM2(__VA_ARGS__)
#define PASS_PARAM4(type, ...) param4, PASS_PARAM3(__VA_ARGS__)
#define PASS_PARAM5(type, ...) param5, PASS_PARAM4(__VA_ARGS__)
#define PASS_PARAM6(type, ...) param6, PASS_PARAM5(__VA_ARGS__)
#define PASS_PARAM7(type, ...) param7, PASS_PARAM6(__VA_ARGS__)
#define PASS_PARAM8(type, ...) param8, PASS_PARAM7(__VA_ARGS__)
#define PASS_PARAM9(type, ...) param9, PASS_PARAM8(__VA_ARGS__)
#define PASS_PARAM10(type, ...) param10, PASS_PARAM9(__VA_ARGS__)
#define PASS_PARAMS(...) \
  BASE_CONCAT(PASS_PARAM, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

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
            root_view)                                                        \
        : ancestor##BuilderT<BuilderT>(root_view) {}                          \
    explicit view_class##BuilderT(                                            \
        std::unique_ptr<                                                      \
            typename ::views::internal::ViewClassTrait<BuilderT>::ViewClass_> \
            view)                                                             \
        : ancestor##BuilderT<BuilderT>(std::move(view)) {}                    \
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

#define GET_VB_MACRO(_1, _2, _3, macro_name, ...) macro_name
#define VIEW_BUILDER_PROPERTY(...)                                          \
  GET_VB_MACRO(__VA_ARGS__, VIEW_BUILDER_PROPERTY3, VIEW_BUILDER_PROPERTY2) \
  (__VA_ARGS__)

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
        decltype((static_cast<void (class_name::*)(__VA_ARGS__)>(             \
            &ViewClass_::method_name))),                                      \
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

// Turn off clang-format due to it messing up the following macro. Places the
// semi-colon on a separate line.
// clang-format off

#define END_VIEW_BUILDER };

// Unlike the above macros, DEFINE_VIEW_BUILDER must be placed in the global
// namespace. Unless 'view_class' is already in the 'views' namespace, it should
// be fully qualified with the namespace in which it lives.

#define DEFINE_VIEW_BUILDER(export, view_class)                         \
namespace views {                                                       \
  template <>                                                           \
  class export Builder<view_class>                                      \
      : public view_class##BuilderT<Builder<view_class>> {              \
   private:                                                             \
    using ViewClass_ = view_class;                                      \
   public:                                                              \
    Builder() = default;                                                \
    explicit Builder(ViewClass_* root_view)                             \
        : view_class##BuilderT<Builder<ViewClass_>>(root_view) {}       \
    explicit Builder(std::unique_ptr<ViewClass_> view)                  \
        : view_class##BuilderT<Builder<ViewClass_>>(std::move(view)) {} \
    Builder(Builder&&) = default;                                       \
    Builder<ViewClass_>& operator=(Builder<ViewClass_>&&) = default;    \
    ~Builder() = default;                                               \
    [[nodiscard]] std::unique_ptr<internal::ViewBuilderCore> Release()  \
        override {                                                      \
      return std::make_unique<Builder<view_class>>(std::move(*this));   \
    }                                                                   \
  };                                                                    \
}  // namespace views

// clang-format on

#endif  // UI_VIEWS_METADATA_VIEW_FACTORY_H_
