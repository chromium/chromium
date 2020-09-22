// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_
#define UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "ui/base/class_property.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/metadata/view_factory_internal.h"
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

  virtual void SetProperty(void* obj) = 0;
};

template <typename TClass, typename TValue, typename TSig, TSig Set>
class PropertySetter : public PropertySetterBase {
 public:
  explicit PropertySetter(metadata::ArgType<TValue> value)
      : value_(std::move(value)) {}
  PropertySetter(const PropertySetter&) = delete;
  PropertySetter& operator=(const PropertySetter&) = delete;
  ~PropertySetter() override = default;

  void SetProperty(void* obj) override {
    (static_cast<TClass*>(obj)->*Set)(std::move(value_));
  }

 private:
  TValue value_;
};

template <typename TClass, typename TValue>
class ClassPropertySetter : public PropertySetterBase {
 public:
  ClassPropertySetter(const ui::ClassProperty<TValue>* property,
                      metadata::ArgType<TValue> value)
      : property_(property), value_(std::move(value)) {}
  ClassPropertySetter(const ClassPropertySetter&) = delete;
  ClassPropertySetter& operator=(const ClassPropertySetter&) = delete;
  ~ClassPropertySetter() override = default;

  void SetProperty(void* obj) override {
    static_cast<TClass*>(obj)->SetProperty(property_, std::move(value_));
  }

 private:
  const ui::ClassProperty<TValue>* property_;
  TValue value_;
};

template <typename TClass, typename TSig, TSig Set>
class ClassMethodCaller : public PropertySetterBase {
 public:
  ClassMethodCaller() = default;
  ClassMethodCaller(const ClassMethodCaller&) = delete;
  ClassMethodCaller& operator=(const ClassMethodCaller&) = delete;
  ~ClassMethodCaller() override = default;

  void SetProperty(void* obj) override { (static_cast<TClass*>(obj)->*Set)(); }

 private:
};

class VIEWS_EXPORT ViewBuilderCore {
 public:
  ViewBuilderCore();
  ViewBuilderCore(ViewBuilderCore&&);
  ViewBuilderCore& operator=(ViewBuilderCore&&);
  virtual ~ViewBuilderCore();

  std::unique_ptr<View> Build();

 protected:
  using ChildList = std::vector<std::reference_wrapper<ViewBuilderCore>>;
  using PropertyList = std::vector<std::unique_ptr<PropertySetterBase>>;

  void AddPropertySetter(std::unique_ptr<PropertySetterBase> setter);
  virtual std::unique_ptr<View> DoBuild() = 0;
  void CreateChildren(View* parent);
  void SetProperties(View* view);

  ChildList children_;
  PropertyList property_list_;
};

template <typename TClass, typename TValue, typename TSig, TSig Set>
class ViewBuilderSetter : public PropertySetterBase {
 public:
  explicit ViewBuilderSetter(std::reference_wrapper<ViewBuilderCore> builder)
      : builder_(std::move(builder)) {}
  ViewBuilderSetter(const ViewBuilderSetter&) = delete;
  ViewBuilderSetter& operator=(const ViewBuilderSetter&) = delete;
  ~ViewBuilderSetter() override = default;

  void SetProperty(void* obj) override {
    (static_cast<TClass*>(obj)->*Set)(builder_.get().Build());
  }

 private:
  std::reference_wrapper<ViewBuilderCore> builder_;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_METADATA_VIEW_FACTORY_INTERNAL_H_
