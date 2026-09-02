/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_UTILS_VALIDATION_UTILS_H_
#define COMMON_UTILS_VALIDATION_UTILS_H_

#include <iterator>
#include <optional>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace iamf_tools {

/*!\brief Returns an error if the size arguments are not equivalent.
 *
 * Intended to be used in OBUs to ensure the reported and actual size of
 * containers are equivalent.
 *
 * \param field_name Field name of the container to insert into the error
 *        message.
 * \param container Container to check the size of.
 * \param reported_size Size reported by associated fields (e.g. "*_size" fields
 *        in the OBU).
 * \return `absl::OkStatus()` if the size arguments are equivalent.
 *         `absl::InvalidArgumentError()` otherwise.
 */
template <typename Container, typename ReportedSize>
absl::Status ValidateContainerSizeEqual(absl::string_view field_name,
                                        const Container& container,
                                        ReportedSize reported_size) {
  const auto actual_size = container.size();
  if (static_cast<ReportedSize>(actual_size) == reported_size) [[likely]] {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Found inconsistency with `", field_name, ".size()`= ", actual_size,
      ". Expected a value of ", reported_size, "."));
}

/*!\brief Returns `absl::OkStatus()` if the arguments are equal.
 *
 * \param lhs First value to compare.
 * \param rhs Second value to compare.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments are equal
 *         `absl::InvalidArgumentError()` if the arguments are not equal.
 */
template <typename T>
absl::Status ValidateEqual(const T& lhs, const T& rhs,
                           absl::string_view context) {
  if (lhs == rhs) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected ", lhs, " == ", rhs, "."));
}

/*!\brief Returns `absl::OkStatus()` if the arguments are not equal.
 *
 * \param lhs First value to compare.
 * \param rhs Second value to compare.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments are not equal
 *         `absl::InvalidArgumentError()` if the arguments are equal.
 */
template <typename T>
absl::Status ValidateNotEqual(const T& lhs, const T& rhs,
                              absl::string_view context) {
  if (lhs != rhs) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected ", lhs, " != ", rhs, "."));
}

/*!\brief Returns `absl::OkStatus()` if the argument has a value.
 *
 * \param argument Argument to check.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments has a value.
 *         `absl::InvalidArgumentError()` if the argument does not have a value.
 */
template <typename T>
absl::Status ValidateHasValue(const std::optional<T>& argument,
                              absl::string_view context) {
  if (argument.has_value()) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected to have a value."));
}

/*!\brief Returns `absl::OkStatus()` if the argument is not NULL.
 *
 * \param argument Argument to check.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments is not NULL.
 *         `absl::InvalidArgumentError()` otherwise.
 */
template <typename T>
absl::Status ValidateNotNull(const T& pointer, absl::string_view context) {
  if (pointer != nullptr) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid pointer: ", context, ". Expected to be not NULL"));
}

/*!\brief Validates that all values in the range [first, last) are unique.
 *
 * \param first Iterator to start from.
 * \param last Iterator to stop before.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if no duplicates are found while iterating.
 *         `absl::InvalidArgumentError()` if duplicates are found.
 */
template <class InputIt>
absl::Status ValidateUnique(InputIt first, InputIt last,
                            absl::string_view context) {
  absl::flat_hash_set<typename std::iterator_traits<InputIt>::value_type>
      seen_values;

  for (auto iter = first; iter != last; ++iter) {
    if (const auto& [unused_iter, inserted] = seen_values.insert(*iter);
        !inserted) {
      return absl::InvalidArgumentError(
          absl::StrCat(context, " must be unique. Found duplicate: ", *iter));
    }
  }
  return absl::OkStatus();
}

/*!\brief Returns `absl::OkStatus()` if `value` is in the range [min, max].
 *
 * \param value Value to check.
 * \param min Minimum allowed value.
 * \param max Maximum allowed value.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the argument is in the range [min, max].
 *         `absl::InvalidArgumentError()` if the argument is not in the range
 *         [min, max].
 */
template <typename T>
absl::Status ValidateInRange(const T& value,
                             std::pair<const T&, const T&> min_max,
                             absl::string_view context) {
  const auto& [min, max] = min_max;
  if (min <= max && value <= max && value >= min) [[likely]] {
    return absl::OkStatus();
  }
  if (min > max) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid range: [", min, ", ", max, "]. Expected min <= max."));
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected ", value, " in range [",
                   min, ", ", max, "]."));
}

/*!\brief Returns `absl::OkStatus()` if `value` (comparison) `reference` is true
 *
 * Useful for arbitrary comparisons, e.g.
 * RETURN_IF_NOT_OK(Validate(my_value, std::greater_equal{}, 0, "my_value >="))
 *
 * \param value Value to check.
 * \param comparison A comparator like std::less.
 * \param reference The value to compare against.
 * \param context Context to insert into the error message for debugging
 *        purposes. For best results, include the operator, e.g. "my_value >=".
 * \return `absl::OkStatus()` if the comparison is true.
 *         `absl::InvalidArgumentError()` otherwise.
 */
template <typename T, typename C>
absl::Status Validate(const T& value, const C& comparison, const T& reference,
                      absl::string_view context) {
  if (comparison(value, reference)) [[likely]] {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Invalid value: ", value, ". Require ", context, reference, "."));
}

}  // namespace iamf_tools

#endif  // COMMON_UTILS_VALIDATION_UTILS_H_
