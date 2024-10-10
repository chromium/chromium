#include <iterator>
#include <utility>
#include <vector>

// Test that reverse iterator are correctly checked when wrapped via some
// constructs like Chromium's `base::ReversedAdapter` iterator.

template <typename T>
class Reversed {
  public:
    explicit Reversed(T& t) : t_(t) {}
    using Iterator = decltype(std::rbegin(std::declval<T&>()));
    Iterator begin() const { return std::rbegin(t_); }
    Iterator end() const { return std::rend(t_); }
  private:
    T& t_;
};  // namespace internal

void ReversedIteratorValid(std::vector<int>& v){
  auto it = Reversed(v).begin();
  if (it == Reversed(v).end()) {
    return;
  }
  *it = 10;
}

void ReversedIteratorInvalid(std::vector<int>& v){
  auto it = Reversed(v).begin();
  // TODO(329133423): This should emit a warning.
  *it = 10;
}

void ReversedIteratorForLoopValid(std::vector<int>& v){
  for(auto& i : Reversed(v)) {
    i = 0;
  }
}

void ReversedIteratorForLoopInvalid(std::vector<int>& v){
  for(auto& i : Reversed(v)) {
    if (i == 0) {
      v.clear(); // TODO(329133423): This should emit a warning.
    }
  }
}