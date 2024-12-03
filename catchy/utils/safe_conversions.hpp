#ifndef CATCHY_UTILS_SAFE_CONVERSIONS_HPP
#define CATCHY_UTILS_SAFE_CONVERSIONS_HPP

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <cmath>

namespace catchy::utils {

template<typename To, typename From>
To safe_cast(From value) {
    static_assert(std::is_arithmetic_v<From> && std::is_arithmetic_v<To>,
                 "safe_cast can only be used with arithmetic types");

    if constexpr (std::is_same_v<To, From>) {
        return value;
    }
    else if constexpr (std::is_floating_point_v<From> && std::is_integral_v<To>) {
        if (value > std::numeric_limits<To>::max() ||
            value < std::numeric_limits<To>::min() ||
            value != std::floor(value)) {
            throw std::overflow_error("Safe cast would lose precision or overflow");
        }
        return static_cast<To>(value);
    }
    else {
        if (value > std::numeric_limits<To>::max() ||
            value < std::numeric_limits<To>::min()) {
            throw std::overflow_error("Safe cast would overflow");
        }
        return static_cast<To>(value);
    }
}

inline uint32_t safe_string_length(const std::string& str) {
    return safe_cast<uint32_t>(str.length());
}

} // namespace catchy::utils

#endif // CATCHY_UTILS_SAFE_CONVERSIONS_HPP
