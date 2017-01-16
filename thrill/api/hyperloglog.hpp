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

#include <cmath>
#include <iostream>
#include <limits.h>

#include <thrill/api/all_reduce.hpp>
#include <thrill/common/functional.hpp>
#include <thrill/api/distribute.hpp>

namespace thrill {
namespace api {
uint64_t siphash(const unsigned char key[16], const unsigned char *m,
                 size_t len);
}

template <typename Value> uint64_t hash(const Value &val) {
    const unsigned char key[16] = {0, 0, 0, 0, 0, 0, 0, 0x4,
                                   0, 0, 0, 0, 0, 0, 0, 0x7};
    return api::siphash(key, reinterpret_cast<const unsigned char *>(&val),
                        sizeof(Value));
}

template <typename ForwardIt, typename Idx, typename Value>
Value getMaximumValue(ForwardIt &cur, ForwardIt end, Idx idx, Value value) {
    for (; cur != end && cur->first == idx; ++cur) {
        value = std::max(value, cur->second);
    }
    return value;
}

enum class RegisterFormat { SPARSE, DENSE };
template <size_t p> struct Registers {
    static const size_t MAX_SPARSELIST_SIZE = 200;
    static const size_t MAX_TMPSET_SIZE = 40;
    RegisterFormat format;
    std::vector<std::pair<size_t, uint64_t>> sparseList;
    std::vector<std::pair<size_t, uint64_t>> tmpSet;
    std::vector<uint64_t> entries;
    Registers() : format(RegisterFormat::SPARSE) { }
    size_t size() const { return entries.size(); }
    void toDense() {
        assert(format == RegisterFormat::SPARSE);
        format = RegisterFormat::DENSE;
        entries.resize(1 << p, 0);
        for (auto &val : sparseList) {
            entries[val.first] = val.second;
        }
        for (auto &val : tmpSet) {
            entries[val.first] = std::max(entries[val.first], val.second);
        }
        sparseList.clear();
        tmpSet.clear();
        sparseList.shrink_to_fit();
        tmpSet.shrink_to_fit();
    }
    template <typename ValueType> void insert(const ValueType &value) {
        // first p bits are the index
        uint64_t hashVal = static_cast<uint64_t>(hash<ValueType>(value));
        uint64_t index = hashVal >> (64 - p);
        uint64_t val = hashVal << p;
        // Check for off-by-one
        // __builtin_clz does not return the correct value for uint64_t
        static_assert(sizeof(long long) * CHAR_BIT == 64,
                      "64 Bit long long are required for hyperloglog.");
        uint64_t leadingZeroes = val == 0 ? (64 - p) : __builtin_clzll(val);
        assert(leadingZeroes >= 0 && leadingZeroes <= (64 - p));
        switch (format) {
        case RegisterFormat::SPARSE:
            tmpSet.emplace_back(index, leadingZeroes + 1);
            if (tmpSet.size() > MAX_TMPSET_SIZE) {
                mergeSparse();
            }
            if (sparseList.size() > MAX_SPARSELIST_SIZE) {
                toDense();
            }
            break;
        case RegisterFormat::DENSE:
            entries[index] = std::max(leadingZeroes + 1, entries[index]);
            break;
        }
    }
    void mergeSparse() {
        assert(std::is_sorted(sparseList.begin(), sparseList.end()));
        std::sort(tmpSet.begin(), tmpSet.end());
        std::vector<std::pair<size_t, uint64_t>> resultVec;
        auto it1 = sparseList.begin();
        auto it2 = tmpSet.begin();
        while (it1 != sparseList.end()) {
            assert(resultVec.empty() || it2 == tmpSet.end() ||
                   resultVec.back().first < it1->first ||
                   resultVec.back().first < it2->first);
            if (it2 == tmpSet.end()) {
                resultVec.insert(resultVec.end(), it1, sparseList.end());
                // This is only done to satisfy the assertion below
                it1 = sparseList.end();
                break;
            } else if (it1->first < it2->first) {
                resultVec.emplace_back(*it1);
                ++it1;
            } else if (it1->first > it2->first) {
                size_t idx = it2->first;
                auto maxVal = getMaximumValue(it2, tmpSet.end(), idx, it2->second);
                resultVec.emplace_back(idx, maxVal);
            } else {
                size_t idx = it1->first;
                auto maxVal = getMaximumValue(it2, tmpSet.end(), idx, it1->second);
                resultVec.emplace_back(idx, maxVal);
                ++it1;
            }
        }
        while (it2 != tmpSet.end()) {
            size_t idx = it2->first;
            auto maxVal = getMaximumValue(it2, tmpSet.end(), idx, it2->second);
            resultVec.emplace_back(idx, maxVal);
        }
        assert(it1 == sparseList.end());
        assert(it2 == tmpSet.end());
        tmpSet.clear();
        tmpSet.shrink_to_fit();
        sparseList = std::move(resultVec);
    }
};

namespace data {
template <typename Archive, size_t p>
struct Serialization<Archive, Registers<p>,
                     typename std::enable_if<p <= 16>::type> {
    static void Serialize(const Registers<p> &x, Archive &ar) {
        Serialization<Archive, RegisterFormat>::Serialize(x.format, ar);
        switch (x.format) {
        case RegisterFormat::SPARSE:
            Serialization<Archive, decltype(x.sparseList)>::Serialize(
                x.sparseList, ar);
            Serialization<Archive, decltype(x.tmpSet)>::Serialize(x.tmpSet, ar);
            break;
        case RegisterFormat::DENSE:
            for (auto it = x.entries.begin(); it != x.entries.end(); ++it) {
                Serialization<Archive, uint64_t>::Serialize(*it, ar);
            }
            break;
        }
    }
    static Registers<p> Deserialize(Archive &ar) {
        Registers<p> out;
        out.format =
            std::move(Serialization<Archive, RegisterFormat>::Deserialize(ar));
        switch (out.format) {
        case RegisterFormat::SPARSE:
            out.sparseList = std::move(
                Serialization<Archive, decltype(out.sparseList)>::Deserialize(
                    ar));
            out.tmpSet = std::move(
                Serialization<Archive, decltype(out.tmpSet)>::Deserialize(ar));
            break;
        case RegisterFormat::DENSE:
            out.entries.resize(1 << p);
            for (size_t i = 0; i != out.size(); ++i) {
                out.entries[i] = std::move(
                    Serialization<Archive, uint64_t>::Deserialize(ar));
            }
            break;
        }
        return out;
    }
    static constexpr bool is_fixed_size = false;
    static constexpr size_t fixed_size = 0;
};
}

namespace api {

extern const std::array<double, 15> thresholds;
extern const std::array<std::vector<double>, 15> rawEstimateData;
extern const std::array<std::vector<double>, 15> biasData;

template <size_t p> constexpr double alpha = 0.7213 / (1 + 1.079 / (1 << p));
template <> constexpr double alpha<4> = 0.673;
template <> constexpr double alpha<5> = 0.697;
template <> constexpr double alpha<6> = 0.709;

template <size_t p> static double threshold() { return thresholds[p - 4]; }

int binarySearch(double rawEstimate, const std::vector<double> &estimatedData);

double knearestNeighbor(int k, int index, double estimate,
                        const std::vector<double> &bias,
                        const std::vector<double> &estimateData);
template <size_t p> static double estimateBias(double rawEstimate) {
    /**
     * 1. Find Elements in rawEstimateData (binary Search)
     * 2. k-nearest neighbor interpolation with k = 6
     * Estimation with: which data? from biasData!
    */
    std::vector<double> estimatedData = rawEstimateData[p - 4];
    int lowerEstimateIndex = binarySearch(rawEstimate, estimatedData);

    std::vector<double> bias = biasData[p - 4];

    return knearestNeighbor(6, lowerEstimateIndex, rawEstimate, bias,
                            estimatedData);
}

template <size_t p>
static Registers<p> combineRegisters(Registers<p> &registers1,
                                     Registers<p> &registers2) {
    if (registers1.format == RegisterFormat::SPARSE &&
        registers2.format == RegisterFormat::DENSE) {
        registers1.toDense();
    } else if (registers1.format == RegisterFormat::DENSE &&
               registers2.format == RegisterFormat::SPARSE) {
        registers2.toDense();
    }
    assert(registers1.format == registers2.format);
    switch (registers1.format) {
    case RegisterFormat::SPARSE:
        registers1.tmpSet.insert(registers1.tmpSet.end(),
                                 registers2.sparseList.begin(),
                                 registers2.sparseList.end());
        registers1.tmpSet.insert(registers1.tmpSet.end(),
                                 registers2.tmpSet.begin(),
                                 registers2.tmpSet.end());
        registers1.mergeSparse();
        if (registers1.sparseList.size() > Registers<p>::MAX_SPARSELIST_SIZE) {
            registers1.toDense();
        }
        break;
    case RegisterFormat::DENSE:
        const size_t m = 1 << p;
        assert(m == registers1.size());
        assert(m == registers2.size());
        for (size_t i = 0; i < m; ++i) {
            registers1.entries[i] =
                std::max(registers1.entries[i], registers2.entries[i]);
        }
        break;
    }
    return registers1;
}

/*!
 * \ingroup api_layer
 */
template <size_t p, typename ValueType>
class HyperLogLogNode final : public ActionResultNode<Registers<p>> {
    static constexpr bool debug = false;

    using Super = ActionResultNode<Registers<p>>;
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

    void PreOp(const ValueType &input) { registers.insert(input); }

    //! Executes the sum operation.
    void Execute() final {
        // process the reduce
        registers = context_.net.AllReduce(registers, combineRegisters<p>);
    }

    //! Returns result of global sum.
    const Registers<p> &result() const final { return registers; }

  private:
    Registers<p> registers;
};

template <typename ValueType, typename Stack>
template <size_t p>
double DIA<ValueType, Stack>::HyperLogLog() const {
    assert(IsValid());

    auto node = common::MakeCounting<HyperLogLogNode<p, ValueType>>(
        *this, "HyperLogLog");
    node->RunScope();
    auto reducedRegisters = node->result();

    if (reducedRegisters.format == RegisterFormat::SPARSE) {
        reducedRegisters.toDense();
    }

    const size_t m = 1 << p;
    assert(reducedRegisters.size() == m);

    DIA<uint64_t> distributedEntries = Distribute<uint64_t>(this->ctx(), reducedRegisters.entries);

    std::pair<double, unsigned int> pairEV = distributedEntries.Map(
        [this](const uint64_t &entry) {
            return std::pair<double, unsigned int>(std::pow(2.0, -static_cast<double>(entry)), entry == 0 ? 1 : 0);
        }
    ).AllReduce(
        [this](const std::pair<double, unsigned int>& valA, const std::pair<double, unsigned int>& valB) {
            return std::pair<double, unsigned int>(valA.first + valB.first, valA.second + valB.second);
        },
        std::pair<double, unsigned int>(0.0, 0)
    );

    double E = pairEV.first;
    unsigned V = pairEV.second;

    E = alpha<p> * m * m / E;
    double E_ = E;
    if (E <= 5 * m) {
        double bias = estimateBias<p>(E);
        E = E - bias;
    }

    double H = E_;
    if (V != 0) {
        // linear count
        std::cout << "linear count\n";
        H = m * log(static_cast<double>(m) / V);
    }

    if (H <= threshold<p>()) {
        return H;
    } else {
        return E_;
    }
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_HYPERLOGLOG_HEADER

/******************************************************************************/
