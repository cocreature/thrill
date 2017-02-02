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
#include <thrill/api/distribute.hpp>
#include <thrill/common/functional.hpp>

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

// The high 25 bit in this register are used for the index, the next 6 bits for
// the value and the last bit is currently unused
using SparseRegister = uint32_t;

enum class RegisterFormat { SPARSE, DENSE };

template <const uint64_t n>
const uint64_t lowerNBitMask = (static_cast<uint64_t>(1) << n) - 1;
template <uint32_t n>
const uint32_t upperNBitMask = ~(static_cast<uint32_t>((1 << (32 - n)) - 1));
template <size_t sparsePrecision, size_t densePrecision>
std::pair<size_t, uint8_t> decodeHash(SparseRegister reg) {
    static_assert(sparsePrecision >= densePrecision,
                  "densePrecision must not be greater than sparsePrecision");
    uint32_t denseValue;
    uint32_t lowestBit = reg & 1;
    uint32_t index;
    if (lowestBit == 1) {
        uint32_t sparseValue = (reg >> 1) & lowerNBitMask<31 - sparsePrecision>;
        denseValue = sparseValue + (sparsePrecision - densePrecision);
        index = (reg >> (32 - densePrecision)) & lowerNBitMask<densePrecision>;
    } else {
        // First zero bottom bits, then shift the bits used for the new index to the
        // left
        uint32_t topBitsValue =
            (reg & upperNBitMask<sparsePrecision>) << densePrecision;
        denseValue = __builtin_clz(topBitsValue) + 1;
        index = (reg >> (32 - densePrecision)) & lowerNBitMask<densePrecision>;
    }
    return {index, denseValue};
}

template <size_t precision>
std::pair<uint32_t, uint8_t> splitSparseRegister(SparseRegister reg) {
    uint8_t value = (reg & lowerNBitMask<32 - precision>) >> 1;
    uint32_t idx = reg >> (32 - precision);
    return {idx, value};
}

template <size_t sparsePrecision, size_t densePrecision> uint32_t encodePair(uint32_t index, uint8_t value) {
    uint32_t decidingBits = lowerNBitMask<sparsePrecision - densePrecision> & index; // x_{63 - densePrecision}...x_{64 - sparsePrecision}
    if (decidingBits == 0) {
        return index << (32 - sparsePrecision) | (value << 1) | 1;
    } else {
        uint32_t shifted = index << (32 - sparsePrecision); // x_63...x_{64 - sparsePrecision} || 0
        return shifted;
    }
}

template <size_t sparsePrecision, size_t densePrecision> uint32_t encodeHash(uint64_t hash) {
    static_assert(sparsePrecision <= 32, "sparse precision must be smaller than 32");
    static_assert(densePrecision < sparsePrecision, "dense precision must be smaller than sparse precision");

    // precision bits are used for the index, the rest is used as the value
    uint64_t valueBits = hash << sparsePrecision;
    static_assert(sizeof(long long) * CHAR_BIT == 64,
                  "64 bit long long are required for hyperloglog.");
    uint8_t leadingZeroes =
        valueBits == 0 ? (64 - sparsePrecision) : __builtin_clzll(valueBits);
    uint32_t index = (hash >> (64 - sparsePrecision));
    auto encoded =
        encodePair<sparsePrecision, densePrecision>(index, leadingZeroes + 1);
    return encoded;
}

template <size_t precision>
std::vector<SparseRegister>
mergeSameIndices(const std::vector<SparseRegister> &sparseList) {
    if (sparseList.empty()) {
        return {};
    }
    auto it = sparseList.begin();
    std::vector<SparseRegister> mergedSparseList = {*it};
    ++it;
    std::pair<size_t, uint8_t> lastEntry =
        splitSparseRegister<precision>(mergedSparseList.back());
    for (; it != sparseList.end(); ++it) {
        auto decoded = splitSparseRegister<precision>(*it);
        assert(decoded.first >= lastEntry.first);
        if (decoded.first > lastEntry.first) {
            mergedSparseList.emplace_back(*it);
        } else {
            assert(decoded.second >= lastEntry.second);
            mergedSparseList.back() = *it;
        }
        lastEntry = decoded;
    }
    return mergedSparseList;
}

class VectorWriter : public common::ItemWriterToolsBase<VectorWriter> {
    std::vector<uint8_t> &buffer;

  public:
    VectorWriter(std::vector<uint8_t> &buffer) : buffer(buffer) {}
    VectorWriter &PutByte(uint8_t data) {
        buffer.emplace_back(data);
        return *this;
    }
};

template <typename ForwardIt>
class SparseListIterator
    : public common::ItemReaderToolsBase<SparseListIterator<ForwardIt>>,
      public std::iterator<std::forward_iterator_tag, uint32_t, std::ptrdiff_t,
                           void, void> {
    ForwardIt iterator;
    uint32_t lastVal = 0;
    static_assert(std::is_same<typename ForwardIt::value_type, uint8_t>::value,
                  "ForwardIt must return uint8_t");

  public:
    explicit SparseListIterator(ForwardIt it) : iterator(it) {}
    uint8_t GetByte() { return *iterator++; }
    SparseListIterator<ForwardIt> &operator++() {
        lastVal = lastVal + this->GetVarint32();
        return *this;
    }
    SparseListIterator<ForwardIt> operator++(int) {
        SparseListIterator<ForwardIt> prev(*this);
        ++*this;
        return prev;
    }
    uint32_t operator*() {
        ForwardIt prevIt = iterator;
        uint32_t val = this->GetVarint32();
        iterator = prevIt;
        return lastVal + val;
    }
    bool operator==(const SparseListIterator<ForwardIt> &other) {
        return iterator == other.iterator;
    }
    bool operator!=(const SparseListIterator<ForwardIt> &other) {
        return !(*this == other);
    }
};

template <typename ForwardIt>
SparseListIterator<ForwardIt> makeSparseListIterator(ForwardIt it) {
    return SparseListIterator<ForwardIt>(it);
}

class DecodedSparseList {
    const std::vector<uint8_t> &sparseListBuffer;

  public:
    DecodedSparseList(const std::vector<uint8_t> &sparseListBuf)
        : sparseListBuffer(sparseListBuf) {}
    auto begin() { return makeSparseListIterator(sparseListBuffer.begin()); }
    auto end() { return makeSparseListIterator(sparseListBuffer.end()); }
};

// Perform a varint and a difference encoding
std::vector<uint8_t> encodeSparseList(const std::vector<uint32_t> &sparseList);

template <size_t p> struct Registers {
    static const size_t MAX_SPARSELIST_SIZE = 200;
    static const size_t MAX_TMPSET_SIZE = 40;
    RegisterFormat format;
    // Register values are always smaller than 64. We thus need log2(64) = 6
    // bits to store them. In particular an uint8_t is sufficient
    std::vector<uint8_t> sparseListBuffer;
    std::vector<SparseRegister> tmpSet;
    std::vector<uint8_t> entries;
    Registers() : format(RegisterFormat::SPARSE) {
        // toDense();
    }
    size_t size() const { return entries.size(); }
    void toDense() {
        assert(format == RegisterFormat::SPARSE);
        format = RegisterFormat::DENSE;
        entries.resize(1 << p, 0);
        for (auto val : DecodedSparseList(sparseListBuffer)) {
            auto decoded = decodeHash<25, p>(val);
            auto entry = entries[decoded.first];
            auto value = std::max(entry, decoded.second);
            entries[decoded.first] = value;
        }
        for (auto &val : tmpSet) {
            auto decoded = decodeHash<25, p>(val);
            auto entry = entries[decoded.first];
            auto value = std::max(entry, decoded.second);
            entries[decoded.first] = value;
        }
        sparseListBuffer.clear();
        tmpSet.clear();
        sparseListBuffer.shrink_to_fit();
        tmpSet.shrink_to_fit();
    }
    template <typename ValueType> void insert(const ValueType &value) {
        // first p bits are the index
        uint64_t hashVal = hash<ValueType>(value);
        static_assert(sizeof(long long) * CHAR_BIT == 64,
                      "64 Bit long long are required for hyperloglog.");
        switch (format) {
        case RegisterFormat::SPARSE:
            tmpSet.emplace_back(encodeHash<25, p>(hashVal));
            if (tmpSet.size() > MAX_TMPSET_SIZE) {
                mergeSparse();
            }
            if (sparseListBuffer.size() > MAX_SPARSELIST_SIZE) {
                toDense();
            }
            break;
        case RegisterFormat::DENSE:
            uint64_t index = hashVal >> (64 - p);
            uint64_t val = hashVal << p;
            uint8_t leadingZeroes = val == 0 ? (64 - p) : __builtin_clzll(val);
            assert(leadingZeroes >= 0 && leadingZeroes <= (64 - p));
            entries[index] =
                std::max<uint8_t>(leadingZeroes + 1, entries[index]);
            break;
        }
    }
    void mergeSparse() {
        DecodedSparseList sparseList(sparseListBuffer);
        assert(std::is_sorted(sparseList.begin(), sparseList.end()));
        std::sort(tmpSet.begin(), tmpSet.end());
        std::vector<SparseRegister> resultVec;
        std::merge(sparseList.begin(), sparseList.end(), tmpSet.begin(),
                   tmpSet.end(), std::back_inserter(resultVec));
        tmpSet.clear();
        tmpSet.shrink_to_fit();
        sparseListBuffer = encodeSparseList(mergeSameIndices<25>(resultVec));
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
            Serialization<Archive, decltype(x.sparseListBuffer)>::Serialize(
                x.sparseListBuffer, ar);
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
            out.sparseListBuffer = std::move(
                Serialization<Archive,
                              decltype(out.sparseListBuffer)>::Deserialize(ar));
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
    case RegisterFormat::SPARSE: {
        DecodedSparseList sparseList2(registers2.sparseListBuffer);
        registers1.tmpSet.insert(registers1.tmpSet.end(), sparseList2.begin(),
                                 sparseList2.end());
        registers1.tmpSet.insert(registers1.tmpSet.end(),
                                 registers2.tmpSet.begin(),
                                 registers2.tmpSet.end());
        registers1.mergeSparse();
        if (registers1.sparseListBuffer.size() >
            Registers<p>::MAX_SPARSELIST_SIZE) {
            registers1.toDense();
        }
        break;
    }
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
        std::cout << "Sparse Format is used for the final output." << std::endl;
        reducedRegisters.mergeSparse();
        const size_t m = 1 << 25; // TODO: proper variable for precision used in sparse format
        unsigned V = m - reducedRegisters.sparseListBuffer.size();
        return m * log(static_cast<double>(m) / V);
    }

    const size_t m = 1 << p;
    assert(reducedRegisters.size() == m);

    DIA<uint8_t> distributedEntries =
        Distribute<uint8_t>(this->ctx(), reducedRegisters.entries);

    std::pair<double, unsigned int> pairEV =
        distributedEntries
            .Map([this](const uint64_t &entry) {
                return std::pair<double, unsigned int>(
                    std::pow(2.0, -static_cast<double>(entry)),
                    entry == 0 ? 1 : 0);
            })
            .AllReduce(
                [this](const std::pair<double, unsigned int> &valA,
                       const std::pair<double, unsigned int> &valB) {
                    return std::pair<double, unsigned int>(
                        valA.first + valB.first, valA.second + valB.second);
                },
                std::pair<double, unsigned int>(0.0, 0));

    double E = pairEV.first;
    unsigned V = pairEV.second;

    E = alpha<p> * m * m / E;
    double E_ = E;
    if (E <= 5 * m) {
        double bias = estimateBias<p>(E);
        E_ = E - bias;
    }

    double H = E_;
    if (V != 0) {
        // linear count
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
