// Exercise the HarfBuzz C++ ownership helpers against the actual C ABI.
#ifdef HR_TEST_SUBSET_FIRST
#include "hr-hb-subset.h"
#endif
#include "hr-hb-cplusplus.hh"
#ifdef HR_TEST_SUBSET_LAST
#include "hr-hb-subset.h"
#endif
// Repeated includes must not redefine the subset vtables.
#include "hr-hb-cplusplus.hh"
#if defined(HR_TEST_SUBSET_FIRST) || defined(HR_TEST_SUBSET_LAST)
#include "hr-hb-subset.h"
#endif

#include <cassert>
#include <unordered_set>

static hb_user_data_key_t key;
static unsigned destroyed;

static void count_destroy(void *data)
{
  assert(data == &destroyed);
  ++destroyed;
}

template <typename T>
static void check_ownership(T *object)
{
  const unsigned before = destroyed;
  assert(object);
  {
    hb::shared_ptr<T> owner(object);
    owner.set_user_data(&key, &destroyed, count_destroy, true);
    assert(owner.get_user_data(&key) == &destroyed);
    {
      hb::shared_ptr<T> copy(owner);
      hb::shared_ptr<T> assigned;
      assigned = copy;
      hb::shared_ptr<T> moved(std::move(copy));
      assert(!copy);
      assert(moved == owner);
      copy = std::move(assigned);
      assert(!assigned);
      assert(copy == owner);
      std::unordered_set<hb::shared_ptr<T>> set;
      set.insert(owner);
      set.insert(moved);
      assert(set.size() == 1);
      assert(destroyed == before);
    }
    assert(destroyed == before);
    hb::unique_ptr<T> unique(owner.reference());
    hb::unique_ptr<T> moved(std::move(unique));
    assert(!unique);
    unique = std::move(moved);
    assert(!moved);
    hb::unique_ptr<T> released(unique.release());
    assert(!unique);
    hb::unique_ptr<T> empty;
    swap(released, empty);
    assert(!released);
    assert(empty.get() == owner.get());
    assert(std::hash<hb::unique_ptr<T>>{}(empty) ==
           std::hash<T *>{}(empty.get()));
  }
  assert(destroyed == before + 1);
}

template <typename T>
static void check_empty()
{
  hb::shared_ptr<T> empty(hb::shared_ptr<T>::get_empty());
  assert(empty);
  hb::shared_ptr<T> copy(empty);
  assert(copy == empty);
}

int main(int argc, char **argv)
{
  static_assert(hb::is_shared_ptr<hb::shared_ptr<hb_blob_t>>::value, "shared trait");
  static_assert(hb::is_unique_ptr<hb::unique_ptr<hb_blob_t>>::value, "unique trait");
  static_assert(!hb::is_shared_ptr<hb_blob_t *>::value, "raw pointer trait");
  static_assert(!std::is_copy_constructible<hb::unique_ptr<hb_blob_t>>::value,
                "unique ownership");
  static_assert(std::is_nothrow_move_constructible<hb::shared_ptr<hb_blob_t>>::value,
                "noexcept move");

  check_empty<hb_blob_t>();
  check_empty<hb_buffer_t>();
  check_empty<hb_face_t>();
  check_empty<hb_font_t>();
  check_empty<hb_font_funcs_t>();
  check_empty<hb_map_t>();
  check_empty<hb_set_t>();
  check_empty<hb_shape_plan_t>();

  check_ownership(hb_blob_create("abc", 3, HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr));
  check_ownership(hb_buffer_create());
  check_ownership(hb_font_funcs_create());
  check_ownership(hb_map_create());
  check_ownership(hb_set_create());

  assert(argc == 2);
  hb::shared_ptr<hb_blob_t> blob(hb_blob_create_from_file_or_fail(argv[1]));
  assert(blob && hb_blob_get_length(blob));
  hb::shared_ptr<hb_face_t> face(hb_face_create(blob, 0));
  assert(hb_face_get_glyph_count(face));
  check_ownership(hb_face_create(blob, 0));
  check_ownership(hb_font_create(face));

  hb_segment_properties_t props = HB_SEGMENT_PROPERTIES_DEFAULT;
  props.direction = HB_DIRECTION_LTR;
  props.script = HB_SCRIPT_LATIN;
  check_ownership(hb_shape_plan_create(face, &props, nullptr, 0, nullptr));

#if defined(HR_TEST_SUBSET_FIRST) || defined(HR_TEST_SUBSET_LAST)
  check_ownership(hb_subset_input_create_or_fail());
  hb::shared_ptr<hb_subset_input_t> input(hb_subset_input_create_or_fail());
  hb_set_add(hb_subset_input_unicode_set(input), 'A');
  check_ownership(hb_subset_plan_create_or_fail(face, input));
  hb::unique_ptr<hb_subset_plan_t> plan(hb_subset_plan_create_or_fail(face, input));
  assert(plan);
  hb::shared_ptr<hb_face_t> result(hb_subset_plan_execute_or_fail(plan));
  assert(result && hb_face_get_glyph_count(result));
  hb::unique_ptr<hb_blob_t> subset_blob(hb_face_reference_blob(result));
  assert(subset_blob && hb_blob_get_length(subset_blob));
#endif
}
