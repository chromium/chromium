// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_
#define UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/views_export.h"

namespace views {

class View;

template <typename T>
class Builder {};

namespace internal {

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
  ClassPropertyMoveSetter(const ui::ClassProperty<TValue*>* property,
                          const TValue& value)
      : property_(property), value_(value) {}
  ClassPropertyMoveSetter(const ui::ClassProperty<TValue*>* property,
                          TValue&& value)
      : property_(property), value_(std::move(value)) {}
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

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_
