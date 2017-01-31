/*******************************************************************************
 * tests/api/hyperloglog_test.cpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2015 Alexander Noe <aleexnoe@gmail.com>
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <thrill/api/generate.hpp>
#include <thrill/api/hyperloglog.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace thrill; // NOLINT

double relativeError(double trueVal, double estimate) {
    return estimate / trueVal - 1;
}
TEST(Operations, HyperLogLog) {
    std::function<void(Context &)> start_func = [](Context &ctx) {
        static constexpr bool debug = true;
        size_t n = 100000;

        auto indices = Generate(ctx, n);

        double estimate = indices.HyperLogLog<4>();
        LOG << "hyperloglog with p=" << 4 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<6>();
        LOG << "hyperloglog with p=" << 6 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<8>();
        LOG << "hyperloglog with p=" << 8 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<10>();
        LOG << "hyperloglog with p=" << 10 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<12>();
        LOG << "hyperloglog with p=" << 12 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<14>();
        LOG << "hyperloglog with p=" << 14 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<16>();
        LOG << "hyperloglog with p=" << 16 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);

        LOG << "###################################################";
        LOG << "hyperloglog for small counts";
        LOG << "";

        n = 1000;

        indices = Generate(ctx, n);

        estimate = indices.HyperLogLog<4>();
        LOG << "hyperloglog with p=" << 4 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<6>();
        LOG << "hyperloglog with p=" << 6 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<8>();
        LOG << "hyperloglog with p=" << 8 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<10>();
        LOG << "hyperloglog with p=" << 10 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<12>();
        LOG << "hyperloglog with p=" << 12 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<14>();
        LOG << "hyperloglog with p=" << 14 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);
        estimate = indices.HyperLogLog<14>();
        LOG << "hyperloglog with p=" << 14 << ": " << estimate
            << ", relative error: " << relativeError(n, estimate);

    };

    thrill::Run([&](thrill::Context &ctx) { start_func(ctx); });
}

TEST(Operations, HyperLogLogMedian) {
    std::function<void(Context &)> start_func = [](Context &ctx) {
        const std::vector<size_t> sampleSizes = {10,    100,    1000,
                                                 10000, 100000, 1000000};
        const size_t iterationsPerSize = 100;

        static std::uniform_int_distribution<size_t> distribution(
            std::numeric_limits<size_t>::min(),
            std::numeric_limits<size_t>::max());
        static std::default_random_engine generator;
        for (auto sampleSize : sampleSizes) {
            std::vector<double> relativeErrors(iterationsPerSize);
            for (size_t i = 0; i < iterationsPerSize; ++i) {
                std::vector<size_t> data;
                if (ctx.my_rank() == 0) {
                    data.resize(sampleSize);
                    std::generate(data.begin(), data.end(),
                                  []() { return distribution(generator); });
                }
                size_t hyperloglogCount =
                    Distribute<size_t>(ctx, data, 0).HyperLogLog<14>();
                std::sort(data.begin(), data.end());
                size_t uniqueCount =
                    std::unique(data.begin(), data.end()) - data.begin();
                relativeErrors[i] =
                    relativeError(uniqueCount, hyperloglogCount);
            }
            std::sort(relativeErrors.begin(), relativeErrors.end());
            if (ctx.my_rank() == 0) {
                double q1 = relativeErrors[relativeErrors.size() * 0.1];
                double q2 = relativeErrors[relativeErrors.size() * 0.5];
                double q3 = relativeErrors[relativeErrors.size() * 0.9];
                std::cout << "Relative error quantiles for sample size "
                          << sampleSize << ":\n"
                          << "10%: " << q1 << ", 50%: " << q2 << ", 90%: " << q3
                          << "\n";
            }
        }

    };

    api::MemoryConfig mem_config;
    mem_config.setup(4 * 1024 * 1024 * 1024llu);
    mem_config.verbose_ = false;

    api::RunLocalMock(mem_config, 1, 2, start_func);
}

TEST(Operations, encodeHash) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, 18446744073709551615u);

    for (int n = 0; n < 1000; ++n) {
        uint64_t random = dis(gen);
        uint32_t index = random >> (64 - 25);
        uint64_t valueBits = random << 25;
        uint8_t value = valueBits == 0 ? (64 - 25) : __builtin_clzll(valueBits);
        value++;
        uint32_t encoded = encodeHash<25>(random);
        auto decoded = splitSparseRegister<25>(encoded);
        ASSERT_EQ(index, decoded.first);
        ASSERT_EQ(value, decoded.second);
    }
}
TEST(Operations, decodeHash) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, 18446744073709551615u);
    uint32_t densePrecision = 4;
    for (int n = 0; n < 1000; ++n) {
        uint64_t random = dis(gen);
        uint32_t index = random >> (64 - densePrecision);
        uint64_t valueBits = random << densePrecision;
        uint8_t value =
            valueBits == 0 ? (64 - densePrecision) : __builtin_clzll(valueBits);
        value++;
        uint32_t encoded = encodeHash<25>(random);
        auto decoded = decodeHash<25, 4>(encoded);
        ASSERT_EQ(index, decoded.first);
        ASSERT_EQ(value, decoded.second);
    }
    densePrecision = 12;
    for (int n = 0; n < 1000; ++n) {
        uint64_t random = dis(gen);
        uint32_t index = random >> (64 - densePrecision);
        uint64_t valueBits = random << densePrecision;
        uint8_t value =
            valueBits == 0 ? (64 - densePrecision) : __builtin_clzll(valueBits);
        value++;
        uint32_t encoded = encodeHash<25>(random);
        auto decoded = decodeHash<25, 12>(encoded);
        ASSERT_EQ(index, decoded.first);
        ASSERT_EQ(value, decoded.second);
    }
}

TEST(Operations, sparseListEncoding) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(
        0, std::numeric_limits<uint32_t>::max());
    std::uniform_int_distribution<size_t> lengthDis(0, 10000);
    for (size_t i = 0; i < 10; ++i) {
        size_t length = lengthDis(gen);
        std::vector<uint32_t> input;
        input.reserve(length);
        for (size_t j = 0; j < length; ++j) {
            input.emplace_back(dis(gen));
        }
        std::sort(input.begin(), input.end());
        std::vector<uint8_t> encoded = encodeSparseList(input);

        std::vector<uint32_t> decoded;
        DecodedSparseList decodedSparseList(encoded);
        for (auto val : decodedSparseList) {
            decoded.emplace_back(val);
        }
        ASSERT_EQ(input, decoded);
    }
}
