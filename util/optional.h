#pragma once

namespace util {
#if __cplusplus > 201703L
template<typename T>
using optional = std::optional<T>;
#else
namespace optional_detail {
struct in_place_t // NOLINT(readability-identifier-naming)
{
  explicit in_place_t() = default;
};
/// \warning This must not be odr-used, as it cannot be made \c inline in C++14.
constexpr in_place_t in_place; // NOLINT(readability-identifier-naming)


namespace detail {
 /// Internal utility to detect trivial copy construction.
 template<typename T> union copy_construction_triviality_helper {
     T t;
     copy_construction_triviality_helper() = default;
     copy_construction_triviality_helper(const copy_construction_triviality_helper&) = default;
     ~copy_construction_triviality_helper() = default;
 };
 /// Internal utility to detect trivial move construction.
 template<typename T> union move_construction_triviality_helper {
     T t;
     move_construction_triviality_helper() = default;
     move_construction_triviality_helper(move_construction_triviality_helper&&) = default;
     ~move_construction_triviality_helper() = default;
 };

 template<class T>
 union trivial_helper {
     T t;
 };
}

template <typename T>
struct is_trivially_copy_constructible
    : std::is_copy_constructible<
          detail::copy_construction_triviality_helper<T>> {};
template <typename T>
struct is_trivially_copy_constructible<T &> : std::true_type {};
template <typename T>
struct is_trivially_copy_constructible<T &&> : std::false_type {};

/// Storage for any type.
//
// The specialization condition intentionally uses
// llvm::is_trivially_copy_constructible instead of
// std::is_trivially_copy_constructible.  GCC versions prior to 7.4 may
// instantiate the copy constructor of `T` when
// std::is_trivially_copy_constructible is instantiated.  This causes
// compilation to fail if we query the trivially copy constructible property of
// a class which is not copy constructible.
//
// The current implementation of optionalStorage insists that in order to use
// the trivial specialization, the value_type must be trivially copy
// constructible and trivially copy assignable due to =default implementations
// of the copy/move constructor/assignment.  It does not follow that this is
// necessarily the case std::is_trivially_copyable is true (hence the expanded
// specialization condition).
//
// The move constructible / assignable conditions emulate the remaining behavior
// of std::is_trivially_copyable.
template <typename T, bool = (is_trivially_copy_constructible<T>::value &&
                              std::is_trivially_copy_assignable<T>::value &&
                              (std::is_trivially_move_constructible<T>::value ||
                               !std::is_move_constructible<T>::value) &&
                              (std::is_trivially_move_assignable<T>::value ||
                               !std::is_move_assignable<T>::value))>
class optionalStorage {
  union {
    char empty;
    T value;
  };
  bool hasVal;

public:
  ~optionalStorage() { reset(); }

  constexpr optionalStorage() noexcept : empty(), hasVal(false) {}
  template <class... Args>
  constexpr explicit optionalStorage(in_place_t, Args &&... args)
      : value(std::forward<Args>(args)...), hasVal(true) {}

  constexpr optionalStorage(optionalStorage const &other) : optionalStorage() {
    if (other.hasValue()) {
      emplace(other.value);
    }
  }
  constexpr optionalStorage(optionalStorage &&other) : optionalStorage() {
    if (other.hasValue()) {
      emplace(std::move(other.value));
    }
  }

  void reset() noexcept {
    if (hasVal) {
      value.~T();
      hasVal = false;
    }
  }

  constexpr bool hasValue() const noexcept { return hasVal; }

  T &getValue() & noexcept {
    assert(hasVal);
    return value;
  }
  constexpr T const &getValue() const & noexcept {
    assert(hasVal);
    return value;
  }
#if LLVM_HAS_RVALUE_REFERENCE_THIS
  T &&getValue() && noexcept {
    assert(hasVal);
    return std::move(value);
  }
#endif

  template <class... Args> void emplace(Args &&... args) {
    reset();
    ::new ((void *)std::addressof(value)) T(std::forward<Args>(args)...);
    hasVal = true;
  }

  optionalStorage &operator=(T const &y) {
    if (hasValue()) {
      value = y;
    } else {
      ::new ((void *)std::addressof(value)) T(y);
      hasVal = true;
    }
    return *this;
  }
  optionalStorage &operator=(T &&y) {
    if (hasValue()) {
      value = std::move(y);
    } else {
      ::new ((void *)std::addressof(value)) T(std::move(y));
      hasVal = true;
    }
    return *this;
  }

  optionalStorage &operator=(optionalStorage const &other) {
    if (other.hasValue()) {
      if (hasValue()) {
        value = other.value;
      } else {
        ::new ((void *)std::addressof(value)) T(other.value);
        hasVal = true;
      }
    } else {
      reset();
    }
    return *this;
  }

  optionalStorage &operator=(optionalStorage &&other) {
    if (other.hasValue()) {
      if (hasValue()) {
        value = std::move(other.value);
      } else {
        ::new ((void *)std::addressof(value)) T(std::move(other.value));
        hasVal = true;
      }
    } else {
      reset();
    }
    return *this;
  }
};

template <typename T> class optionalStorage<T, true> {
  union {
    char empty;
    T value;
  };
  bool hasVal = false;

public:
  ~optionalStorage() = default;

  constexpr optionalStorage() noexcept : empty{} {}

  template <class... Args>
  constexpr explicit optionalStorage(in_place_t, Args &&... args)
      : value(std::forward<Args>(args)...), hasVal(true) {}

  constexpr optionalStorage(optionalStorage const &other) = default;
  constexpr optionalStorage(optionalStorage &&other) = default;

  optionalStorage &operator=(optionalStorage const &other) = default;
  optionalStorage &operator=(optionalStorage &&other) = default;

  void reset() noexcept {
    if (hasVal) {
      value.~T();
      hasVal = false;
    }
  }

  constexpr bool hasValue() const noexcept { return hasVal; }

  T &getValue() & noexcept {
    assert(hasVal);
    return value;
  }
  constexpr T const &getValue() const & noexcept {
    assert(hasVal);
    return value;
  }
#if LLVM_HAS_RVALUE_REFERENCE_THIS
  T &&getValue() && noexcept {
    assert(hasVal);
    return std::move(value);
  }
#endif

  template <class... Args> void emplace(Args &&... args) {
    reset();
    ::new ((void *)std::addressof(value)) T(std::forward<Args>(args)...);
    hasVal = true;
  }

  optionalStorage &operator=(T const &y) {
    if (hasValue()) {
      value = y;
    } else {
      ::new ((void *)std::addressof(value)) T(y);
      hasVal = true;
    }
    return *this;
  }
  optionalStorage &operator=(T &&y) {
    if (hasValue()) {
      value = std::move(y);
    } else {
      ::new ((void *)std::addressof(value)) T(std::move(y));
      hasVal = true;
    }
    return *this;
  }
};

} // namespace optional_detail

template <typename T> class optional {
  optional_detail::optionalStorage<T> Storage;

public:
  using value_type = T;

  constexpr optional() {}
//  constexpr optional(NoneType) {}

  constexpr optional(const T &y) : Storage(optional_detail::in_place, y) {}
  constexpr optional(const optional &O) = default;

  constexpr optional(T &&y) : Storage(optional_detail::in_place, std::move(y)) {}
  constexpr optional(optional &&O) = default;

  template <typename... ArgTypes>
  constexpr optional(optional_detail::in_place_t, ArgTypes &&...Args)
      : Storage(optional_detail::in_place, std::forward<ArgTypes>(Args)...) {}

  optional &operator=(T &&y) {
    Storage = std::move(y);
    return *this;
  }
  optional &operator=(optional &&O) = default;

  /// Create a new object by constructing it in place with the given arguments.
  template <typename... ArgTypes> void emplace(ArgTypes &&... Args) {
    Storage.emplace(std::forward<ArgTypes>(Args)...);
  }

  static constexpr optional create(const T *y) {
    return y ? optional(*y) : optional();
  }

  optional &operator=(const T &y) {
    Storage = y;
    return *this;
  }
  optional &operator=(const optional &O) = default;

  void reset() { Storage.reset(); }

  constexpr const T *getPointer() const { return &Storage.getValue(); }
  T *getPointer() { return &Storage.getValue(); }
  constexpr const T &getValue() const & {
    return Storage.getValue();
  }
  T &getValue() & { return Storage.getValue(); }

  constexpr explicit operator bool() const { return hasValue(); }
  constexpr bool hasValue() const { return Storage.hasValue(); }
  constexpr const T *operator->() const { return getPointer(); }
  T *operator->() { return getPointer(); }
  constexpr const T &operator*() const & {
    return getValue();
  }
  T &operator*() & { return getValue(); }

  template <typename U>
  constexpr T getValueOr(U &&value) const & {
    return hasValue() ? getValue() : std::forward<U>(value);
  }

  T &&getValue() && { return std::move(Storage.getValue()); }
  T &&operator*() && { return std::move(Storage.getValue()); }

  template <typename U>
  T getValueOr(U &&value) && {
    return hasValue() ? std::move(getValue()) : std::forward<U>(value);
  }
};
#endif

template <typename T, typename U>
constexpr bool operator==(const optional<T> &X, const optional<U> &Y) {
  if (X && Y)
    return *X == *Y;
  return X.hasValue() == Y.hasValue();
}

template <typename T, typename U>
constexpr bool operator!=(const optional<T> &X, const optional<U> &Y) {
  return !(X == Y);
}

template <typename T, typename U>
constexpr bool operator<(const optional<T> &X, const optional<U> &Y) {
  if (X && Y)
    return *X < *Y;
  return X.hasValue() < Y.hasValue();
}

template <typename T, typename U>
constexpr bool operator<=(const optional<T> &X, const optional<U> &Y) {
  return !(Y < X);
}

template <typename T, typename U>
constexpr bool operator>(const optional<T> &X, const optional<U> &Y) {
  return Y < X;
}

template <typename T, typename U>
constexpr bool operator>=(const optional<T> &X, const optional<U> &Y) {
  return !(X < Y);
}

template <typename T>
constexpr bool operator==(const optional<T> &X, const T &Y) {
  return X && *X == Y;
}

template <typename T>
constexpr bool operator==(const T &X, const optional<T> &Y) {
  return Y && X == *Y;
}

template <typename T>
constexpr bool operator!=(const optional<T> &X, const T &Y) {
  return !(X == Y);
}

template <typename T>
constexpr bool operator!=(const T &X, const optional<T> &Y) {
  return !(X == Y);
}

template <typename T>
constexpr bool operator<(const optional<T> &X, const T &Y) {
  return !X || *X < Y;
}

template <typename T>
constexpr bool operator<(const T &X, const optional<T> &Y) {
  return Y && X < *Y;
}

template <typename T>
constexpr bool operator<=(const optional<T> &X, const T &Y) {
  return !(Y < X);
}

template <typename T>
constexpr bool operator<=(const T &X, const optional<T> &Y) {
  return !(Y < X);
}

template <typename T>
constexpr bool operator>(const optional<T> &X, const T &Y) {
  return Y < X;
}

template <typename T>
constexpr bool operator>(const T &X, const optional<T> &Y) {
  return Y < X;
}

template <typename T>
constexpr bool operator>=(const optional<T> &X, const T &Y) {
  return !(X < Y);
}

template <typename T>
constexpr bool operator>=(const T &X, const optional<T> &Y) {
  return !(X < Y);
}
}
