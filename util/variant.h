#pragma once

#include <variant>

namespace util {
#if __cplusplus > 201703L
template<typename... Ts>
using variant = std::variant<Ts>;
#else

#endif
}
