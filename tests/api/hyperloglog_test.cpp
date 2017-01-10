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
    };

    thrill::Run([&](thrill::Context &ctx) { start_func(ctx); });
}
