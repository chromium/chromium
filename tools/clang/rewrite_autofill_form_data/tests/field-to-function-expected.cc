// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

namespace autofill {

struct FormFieldData {
  FormFieldData();
  FormFieldData(const FormFieldData&);
  FormFieldData& operator=(const FormFieldData&);
  std::u16string value;
  int something_else;
};

// References in compiler-generated code must not be rewritten.
FormFieldData::FormFieldData() = default;
FormFieldData::FormFieldData(const FormFieldData&) = default;
FormFieldData& FormFieldData::operator=(const FormFieldData&) = default;

class AutofillField : public FormFieldData {};

}  // namespace autofill

namespace testing {

template <typename Class, typename FieldType>
void Field(FieldType Class::*field, int matcher) {}

template <typename Class, typename FieldType>
void Property(FieldType Class::*field(), int matcher) {}

}  // namespace testing

namespace base {

std::u16string ASCIIToUTF16(std::string x) {
  return u"arbitrary string";
}

}  // namespace base

namespace not_autofill {

struct FormFieldData {
  std::u16string value;
  int something_else;
};

}  // namespace not_autofill

// Tests that a read reference `f.value` is replaced with `r.value()`.
std::u16string FunRead() {
  autofill::FormFieldData f;
  return f.value();
}

// Tests that `f.value` at the left of an assignment is replaced with
// `f.set_value(rhs)`.
void FunWrite() {
  ::autofill::FormFieldData f;
  f.set_value(u"foo");
}

// Tests that a read reference `f->value` is replaced with `r->value()`.
std::u16string FunReadPointer() {
  autofill::FormFieldData f;
  autofill::FormFieldData* g = &f;
  return g->value();
}

// Tests that `f->value` at the left of an assignment is replaced with
// `f->set_value(rhs)`.
void FunWritePointer() {
  autofill::FormFieldData f;
  autofill::FormFieldData* g = &f;
  g->set_value(u"foo");
}

// Tests that a read reference `f.value()` in a member function is replaced with
// `f.value()`.
class Class {
  static const std::u16string& value(const autofill::FormFieldData& f) {
    return f.value();
  }
};

// Tests that a references at the left and right hand side of an assignment aer
// replaced appropriately.
void FunReadAndWrite() {
  ::autofill::FormFieldData f;
  ::autofill::FormFieldData g;
  f.set_value(g.value());
}

// Like FunReadAndWrite() but additionally tests that a constness doesn't affect
// the rewriting.
void FunReadConstAndWrite() {
  ::autofill::FormFieldData f;
  const ::autofill::FormFieldData g;
  f.set_value(g.value());
}

// Like FunReadConstAndWrite() but additionally tests that redundant
// parentheses doesn't affect the rewriting.
void FunReadConstAndWriteWithParentheses() {
  ::autofill::FormFieldData f;
  const ::autofill::FormFieldData g;
  ((f).set_value)(((g).value()));
}

// Like FunReadConstAndWrite() but additionally tests that additional whitespace
// doesn't affect the rewriting of a read reference and that the comments
// survive the rewriting.
void FunReadWithWhitespace() {
  autofill::FormFieldData f;
  std::u16string s = f         // comment 1
                         .     // comment 2
                     value();  // comment 3
}

// Like FunReadConstAndWrite() but additionally tests that additional
// whitespace doesn't affect the rewriting of a write reference and that the
// comments survive the rewriting.
void FunWriteWithWhitespace() {
  autofill::FormFieldData f;
  f              // comment 1
      .          // comment 2
      set_value  // comment 3
                 // comment 4
      (u"foo");
}

// Tests whether explicit `operator=()` is rewritten. This is desirable but
// currently not implemented.
void FunWriteExplicitOperator() {
  ::autofill::FormFieldData f;
  f.value().operator=(u"foo");  // Currently not properly rewritten.
}

namespace autofill {

// Tests that references are rewritten even if the object's type isn't fully
// qualified.
std::u16string FunReadImplicitNamespace() {
  FormFieldData f;
  return f.value();
}

// Tests that a reference at the left of an assignment is replaced with
// set_value(), and that a more complex right-hand side experission is
// preserved.
void FunWriteCallExpr() {
  std::string value;
  FormFieldData field;
  field.set_value(base::ASCIIToUTF16(value));
}

}  // namespace autofill

// Like FunWriteCallExpr() but with a more complex expression at the right-hand
// side. It involves UTF-16 string literals to test that the rewriter uses the
// correct clang::LangOptions (and not the default argument).
void FunWriteComplexCallExpr() {
  auto g = [](const std::u16string& s) { return s; };
  autofill::FormFieldData f;
  f.set_value(u"bar" + g(std::u16string(u"foo") + u"qux"));
}

// Tests that the replacement does not touch classes from other namespaces.
std::u16string FunReadNotAutofill() {
  ::not_autofill::FormFieldData f;
  return f.value;
}

// Tests that write references on members of a derived class are rewritten.
void FunWriteDerived() {
  autofill::AutofillField f;
  f.set_value(u"bar");
}

// Tests that read references on vector elements are rewritten.
std::u16string FunReadVector() {
  std::vector<autofill::FormFieldData> fields;
  fields.emplace_back();
  return fields[0].value();
}

// Tests that write references on vector elements are rewritten.
void FunWriteVector() {
  std::vector<autofill::FormFieldData> fields;
  fields.emplace_back();
  fields[0].set_value(u"foo");
}

// Tests that pointers to members are not rewritten.
std::u16string FunFieldPointer() {
  using autofill::FormFieldData;
  FormFieldData f;
  std::u16string FormFieldData::*ptr = &FormFieldData::value;
  return f.*ptr;
}

// Tests that Field(&Class::member, ...) is replaced with
// Property(&Class::member, ...).
void FunMatcher() {
  ::testing::Property(&autofill::FormFieldData::value, {});
  ::testing::Field(&autofill::FormFieldData::something_else, {});
  ::testing::Field(&not_autofill::FormFieldData::value, {});
  ::testing::Field(&not_autofill::FormFieldData::something_else, {});
  using ::testing::Field;
  using ::testing::Property;
  Property(&autofill::FormFieldData::value, {});
  Field(&autofill::FormFieldData::something_else, {});
  Field(&not_autofill::FormFieldData::value, {});
  Field(&not_autofill::FormFieldData::something_else, {});
}
