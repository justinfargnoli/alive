#pragma once

#include <map>
#include <tuple>

namespace util {

namespace detail {
template<typename T>
constexpr std::enable_if_t<std::is_unsigned<T>{}, T>
gcd(T m, T n) {
  return n == 0 ? m : detail::gcd(n, m % n);
}
}

template<typename T, typename U>
constexpr typename std::enable_if<std::is_signed<T>::value
                               && std::is_signed<U>::value,
                                  std::common_type_t<T, U>>::type
gcd(T m, U n) {
  using _Rp = std::common_type_t<T,U>;
  using _Wp = std::make_unsigned_t<_Rp>;
  return static_cast<_Rp>(detail::gcd(static_cast<_Wp>(abs(m)),
                                      static_cast<_Wp>(abs(n))));
}

template<typename T, typename U>
constexpr typename std::enable_if<std::is_unsigned<T>::value
                               && std::is_unsigned<U>::value,
                                  std::common_type_t<T, U>>::type
gcd(T m, U n) {
  using _Rp = std::common_type_t<T,U>;
  using _Wp = std::make_unsigned_t<_Rp>;
  return static_cast<_Rp>(detail::gcd(static_cast<_Wp>(m),
                                      static_cast<_Wp>(n)));
}


namespace detail {

template <typename Map, typename Key, typename... Args>
std::pair<typename Map::iterator, bool> map_try_emplace_impl(Map& map,
                                                             Key&& key,
                                                             Args&&... args) {
  auto lower = map.lower_bound(key);
  if (lower != map.end() && !map.key_comp()(key, lower->first)) {
    // key already exists, do nothing.
    return {lower, false};
  }
  // key did not yet exist, insert it.
  return {map.emplace_hint(lower, std::piecewise_construct,
                           std::forward_as_tuple(std::forward<Key>(key)),
                           std::forward_as_tuple(std::forward<Args>(args)...)),
          true};
}

template <typename Map, typename Key, typename... Args>
typename Map::iterator map_try_emplace_impl(Map& map,
                                            typename Map::const_iterator hint,
                                            Key&& key,
                                            Args&&... args) {
  auto &&key_comp = map.key_comp();
  if ((hint == map.begin() || key_comp(std::prev(hint)->first, key))) {
    if (hint == map.end() || key_comp(key, hint->first)) {
      // *(hint - 1) < key < *hint => key did not exist and hint is correct.
      return map.emplace_hint(
          hint, std::piecewise_construct,
          std::forward_as_tuple(std::forward<Key>(key)),
          std::forward_as_tuple(std::forward<Args>(args)...));
    }
    if (!key_comp(hint->first, key)) {
      // key == *hint => no-op, return correct hint.
      return ConstCastIterator(map, hint);
    }
  }
  // hint was not helpful, dispatch to hintless version.
  return map_try_emplace_impl(map, std::forward<Key>(key),
                              std::forward<Args>(args)...)
        .first;
}

}

template <typename Map, typename... Args>
std::pair<typename Map::iterator, bool>
map_try_emplace(Map& map, const typename Map::key_type& key, Args&&... args) {
  return detail::map_try_emplace_impl(map, key, std::forward<Args>(args)...);
}
template <typename Map, typename... Args>
std::pair<typename Map::iterator, bool>
map_try_emplace(Map& map,
                typename Map::key_type&& key,
                Args&&... args) {
  return detail::map_try_emplace_impl(map, std::move(key),
                                      std::forward<Args>(args)...);
}

}
