// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_VIEW_FACTORY_H_
#define UI_VIEWS_METADATA_VIEW_FACTORY_H_

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/views_export.h"

namespace views {

template <typename Builder>
class BaseViewBuilderT : public internal::ViewBuilderCore {
 public:
  using ViewClass_ = typename internal::ViewClassTrait<Builder>::ViewClass_;
  using ConfigureCallback = base::OnceCallback<void(ViewClass_*)>;
  BaseViewBuilderT() { view_ = std::make_unique<ViewClass_>(); }
  explicit BaseViewBuilderT(std::unique_ptr<ViewClass_> view) {
    view_ = std::move(view);
  }
  explicit BaseViewBuilderT(ViewClass_* root_view) : root_view_(root_view) {}
  BaseViewBuilderT(BaseViewBuilderT&&) = default;
  BaseViewBuilderT& operator=(BaseViewBuilderT&&) = default;
  ~BaseViewBuilderT() override = default;

  template <typename View>
  Builder& CopyAddressTo(View** view_address) & {
    *view_address = view_ ? view_.get() : root_view_.get();
    return *static_cast<Builder*>(this);
  }

  template <typename View>
  Builder&& CopyAddressTo(View** view_address) && {
    return std::move(this->CopyAddressTo(view_address));
  }

  template <typename View>
  Builder& CopyAddressTo(raw_ptr<View>* view_address) & {
    *view_address = view_ ? view_.get() : root_view_.get();
    return *static_cast<Builder*>(this);
  }

  template <typename View>
  Builder&& CopyAddressTo(raw_ptr<View>* view_address) && {
    return std::move(this->CopyAddressTo(view_address));
  }

  Builder& CustomConfigure(ConfigureCallback configure_callback) & {
    configure_callback_ = std::move(configure_callback);
    return *static_cast<Builder*>(this);
  }

  Builder&& CustomConfigure(ConfigureCallback configure_callback) && {
    return std::move(this->CustomConfigure(std::move(configure_callback)));
  }

  template <typename Child>
  Builder& AddChild(Child&& child) & {
    children_.emplace_back(std::make_pair(child.Release(), absl::nullopt));
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

  std::unique_ptr<ViewClass_> Build() && WARN_UNUSED_RESULT {
    DCHECK(!root_view_) << "Root view specified. Use BuildChildren() instead.";
    DCHECK(view_);
    SetProperties(view_.get());
    DoCustomConfigure(view_.get());
    CreateChildren(view_.get());
    return std::move(view_);
  }

  void BuildChildren() && {
    DCHECK(!view_) << "Default constructor called. Use Build() instead.";
    DCHECK(root_view_);
    SetProperties(root_view_);
    DoCustomConfigure(root_view_);
    CreateChildren(root_view_);
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
      children_.emplace_back(std::make_pair(child->Release(), absl::nullopt));
    return *static_cast<Builder*>(this);
  }

  void DoCustomConfigure(ViewClass_* view) {
    if (configure_callback_)
      std::move(configure_callback_).Run(view);
  }

  std::unique_ptr<View> DoBuild() override { return std::move(*this).Build(); }

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
    Builder<ViewClass_>() = default;                                    \
    explicit Builder<ViewClass_>(ViewClass_* root_view)                 \
        : view_class##BuilderT<Builder<ViewClass_>>(root_view) {}       \
    explicit Builder<ViewClass_>(std::unique_ptr<ViewClass_> view)      \
        : view_class##BuilderT<Builder<ViewClass_>>(std::move(view)) {} \
    Builder<ViewClass_>(Builder&&) = default;                           \
    Builder<ViewClass_>& operator=(Builder<ViewClass_>&&) = default;    \
    ~Builder<ViewClass_>() = default;                                   \
    std::unique_ptr<internal::ViewBuilderCore> Release() override       \
        WARN_UNUSED_RESULT {                                            \
      return std::make_unique<Builder<view_class>>(std::move(*this));   \
    }                                                                   \
  };                                                                    \
}  // namespace views

// clang-format on

#endif  // UI_VIEWS_METADATA_VIEW_FACTORY_H_
