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

    int leadingZeros(uint64_t val) {
         uint32_t lower = val; // lower
         uint32_t upper = val >> 32;

        if (upper == 0) {
          return 32 + __builtin_clz(lower);
        } else {
          return __builtin_clz(upper);
        }
    }

// This siphash imlementation is taken from
// https://github.com/floodyberry/siphash
#define ROTL64(a, b) (((a) << (b)) | ((a) >> (64 - b)))
static uint64_t U8TO64_LE(const unsigned char *p) {
    return *(const uint64_t *)p;
}

uint64_t siphash(const unsigned char key[16], const unsigned char *m,
                 size_t len) {
    uint64_t v0, v1, v2, v3;
    uint64_t mi, k0, k1;
    uint64_t last7;
    size_t i, blocks;

    k0 = U8TO64_LE(key + 0);
    k1 = U8TO64_LE(key + 8);
    v0 = k0 ^ 0x736f6d6570736575ull;
    v1 = k1 ^ 0x646f72616e646f6dull;
    v2 = k0 ^ 0x6c7967656e657261ull;
    v3 = k1 ^ 0x7465646279746573ull;

    last7 = (uint64_t)(len & 0xff) << 56;

#define sipcompress()                                                          \
    v0 += v1;                                                                  \
    v2 += v3;                                                                  \
    v1 = ROTL64(v1, 13);                                                       \
    v3 = ROTL64(v3, 16);                                                       \
    v1 ^= v0;                                                                  \
    v3 ^= v2;                                                                  \
    v0 = ROTL64(v0, 32);                                                       \
    v2 += v1;                                                                  \
    v0 += v3;                                                                  \
    v1 = ROTL64(v1, 17);                                                       \
    v3 = ROTL64(v3, 21);                                                       \
    v1 ^= v2;                                                                  \
    v3 ^= v0;                                                                  \
    v2 = ROTL64(v2, 32);

    for (i = 0, blocks = (len & ~7); i < blocks; i += 8) {
        mi = U8TO64_LE(m + i);
        v3 ^= mi;
        sipcompress() sipcompress() v0 ^= mi;
    }

    switch (len - blocks) {
    case 7:
        last7 |= (uint64_t)m[i + 6] << 48;
    case 6:
        last7 |= (uint64_t)m[i + 5] << 40;
    case 5:
        last7 |= (uint64_t)m[i + 4] << 32;
    case 4:
        last7 |= (uint64_t)m[i + 3] << 24;
    case 3:
        last7 |= (uint64_t)m[i + 2] << 16;
    case 2:
        last7 |= (uint64_t)m[i + 1] << 8;
    case 1:
        last7 |= (uint64_t)m[i + 0];
    case 0:
    default:;
    };
    v3 ^= last7;
    sipcompress() sipcompress() v0 ^= last7;
    v2 ^= 0xff;
    sipcompress() sipcompress() sipcompress() sipcompress() return v0 ^ v1 ^
        v2 ^ v3;
}

template <typename Value> uint64_t hash(const Value &val) {
    const unsigned char key[16] = {0, 0, 0, 0, 0, 0, 0, 0x4,
                                   0, 0, 0, 0, 0, 0, 0, 0x7};
    return siphash(
        key, reinterpret_cast<const unsigned char *>(&val), sizeof(Value));
}

// TODO: check that it was not used anywhere
// template <const uint32_t p>
// const uint32_t indexMask = ~(static_cast<uint32_t>((1 << (32 - p)) - 1));

template <size_t p> constexpr double alpha = 0.7213 / (1 + 1.079 / (1 << p));
template <> constexpr double alpha<4> = 0.673;
template <> constexpr double alpha<5> = 0.697;
template <> constexpr double alpha<6> = 0.709;

// TODO We should wrap this in a separate type but I’m too lazy to figure
// out the serialization right now
template <size_t p> using Registers = std::array<uint64_t, p>;

template <typename ValueType, size_t p>
static void insertInRegisters(Registers<1 << p> &registers,
                              const ValueType &value) {
    // first p bits are the index
    uint64_t hashVal = static_cast<uint64_t>(hash<ValueType>(value));
    uint64_t index = hashVal >> (64 - p);
    uint64_t val = hashVal << p;
    // Check for off-by-one
    // __builtin_clz does not return the correct value for uint64_t
    uint64_t leadingZeroes = val == 0 ? (64 - p) : leadingZeros(val);
    assert(leadingZeroes >= 0 && leadingZeroes <= (64 - p));
    registers[index] = std::max(leadingZeroes + 1, registers[index]);
}

template <size_t p>
static Registers<1 << p> combineRegisters(Registers<1 << p> &registers1,
                                          const Registers<1 << p> &registers2) {
    const size_t m = 1 << p;
    assert(m == registers1.size());
    assert(m == registers2.size());
    for (size_t i = 0; i < m; ++i) {
        registers1[i] = std::max(registers1[i], registers2[i]);
    }
    return registers1;
}

/*!
 * \ingroup api_layer
 */
template <size_t p, typename ValueType>
class HyperLogLogNode final : public ActionResultNode<Registers<1 << p>> {
    static constexpr bool debug = false;

    using Super = ActionResultNode<Registers<1 << p>>;
    using Super::context_;

  public:
    template <typename ParentDIA>
    HyperLogLogNode(const ParentDIA &parent, const char *label)
        : Super(parent.ctx(), label, {parent.id()}, {parent.node()}),
          registers{} {
        // Hook PreOp(s)
        auto pre_op_fn = [this](const ValueType &input) { PreOp(input); };

        auto lop_chain = parent.stack().push(pre_op_fn).fold();
        parent.node()->AddChild(this, lop_chain);
    }

    void PreOp(const ValueType &input) {
        insertInRegisters<ValueType, p>(registers, input);
    }

    //! Executes the sum operation.
    void Execute() final {
        // process the reduce
        registers = context_.net.AllReduce(registers, combineRegisters<p>);
    }

    //! Returns result of global sum.
    const Registers<1 << p> &result() const final { return registers; }

  private:
    Registers<1 << p> registers;
};

template <typename ValueType, typename Stack>
template <size_t p>
double DIA<ValueType, Stack>::HyperLogLog() const {
    assert(IsValid());

    auto node =
        common::MakeCounting<HyperLogLogNode<p, ValueType>>(*this, "AllReduce");
    node->RunScope();
    const auto &reducedRegisters = node->result();

    const size_t m = 1 << p;
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
    } else {
        std::cout << "medium/large E\n";
        return E;
    }
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_HYPERLOGLOG_HEADER

/******************************************************************************/
