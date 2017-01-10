/*******************************************************************************
 * thrill/api/hyperloglog.hpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2016 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#ifndef THRILL_API_HYPERLOGLOG_HEADER
#define THRILL_API_HYPERLOGLOG_HEADER

#include <thrill/api/all_reduce.hpp>
#include <thrill/common/functional.hpp>

#include <iostream>

namespace thrill {
namespace api {

template <typename Value> uint32_t hash(const Value &value);

template <> uint32_t hash<size_t>(const size_t &val) {
    // Guaranteed to be good because I found it on stackoverflow
    return val * 2654435761;
}

template <const unsigned int p> const uint32_t indexMask = ~(1 << (32 - p) - 1);

template <size_t p> constexpr double alpha = 0.7213 / (1 + 1.079 / (1 << p));
template <> constexpr double alpha<4> = 0.673;
template <> constexpr double alpha<5> = 0.697;
template <> constexpr double alpha<6> = 0.709;

// TODO We should wrap this in a separate type but I’m too lazy to figure
// out the serialization right now
using Registers = std::vector<uint64_t>;

template <typename ValueType, size_t p>
static Registers registersForValue(const ValueType &value) {
    std::vector<uint64_t> entries(1 << p, 0);
    // first p bits are the index
    uint32_t truncatedHash = static_cast<uint32_t>(hash<ValueType>(value));
    uint32_t index = (truncatedHash & indexMask<p>) >> (32 - p);
    uint32_t val =  truncatedHash << p;
    // Check for off-by-one
    uint64_t leadingZeroes = val == 0 ? (32 - p) : __builtin_clz(val);
    assert(leadingZeroes >= 0 && leadingZeroes <= (32 - p));
    entries[index] = leadingZeroes + 1;
    return entries;
}

template <size_t p>
static Registers combineRegisters(Registers registers1, Registers registers2) {
    assert(registers1.size() == registers2.size());
    size_t m = 1 << p;
    assert(m == registers1.size());
    std::vector<uint64_t> entries(m, 0);
    for (size_t i = 0; i < m; ++i) {
        entries[i] = std::max(registers1[i], registers2[i]);
    }
    return entries;
}

template <size_t p> static Registers emptyRegisters() {
    std::vector<uint64_t> entries(1 << p, 0);
    return entries;
}

template <typename ValueType, typename Stack>
template <size_t p>
double DIA<ValueType, Stack>::HyperLogLog() const {
    assert(IsValid());

    auto reducedRegisters =
        this->Map<std::function<Registers(ValueType)>>(
                registersForValue<ValueType, p>)
            .AllReduce<std::function<Registers(Registers, Registers)>>(
                combineRegisters<p>, emptyRegisters<p>());

    size_t m = 1 << p;
    double E = 0;
    unsigned V = 0;
    assert(reducedRegisters.size() == m);
    for (size_t i = 0; i < m; ++i) {
        E += std::pow(2.0, -static_cast<double>(reducedRegisters[i]));
        if (reducedRegisters[i] == 0) {
            V++;
        }
    }
    E = alpha<p> * m * m / E;

    if (E <= 5.0 / 2 * m) {
        std::cout << "small E\n";
        // linear count
        if (V != 0) {
            // linear count
            std::cout << "linear count\n";
            return m * log(static_cast<double>(m) / V);
        } else {
            return E;
        }
    } else if (E <= 1.0 / 30 * std::pow(2.0, 32)) {
        std::cout << "medium E\n";
        return E;
    } else {
        std::cout << "large E\n";
        return -std::pow(2.0, 32) * log(1 - E / (std::pow(2.0, 32)));
    }
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_HYPERLOGLOG_HEADER

/******************************************************************************/
