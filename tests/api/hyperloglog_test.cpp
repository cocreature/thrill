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

#include <thrill/api/hyperloglog.hpp>
#include <thrill/api/generate.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace thrill; // NOLINT

TEST(Operations, HyperLogLog) {
    std::function<void(Context &)> start_func = [](Context &ctx) {
        static constexpr bool debug = true;
        size_t n = 1000000;

        auto indices = Generate(ctx, n);

        LOG << "hyperloglog with p=" << 4 << ": " << indices.HyperLogLog<4>();
        LOG << "hyperloglog with p=" << 8 << ": " << indices.HyperLogLog<8>();
        LOG << "hyperloglog with p=" << 12 << ": " << indices.HyperLogLog<12>();
        // LOG << "hyperloglog with p=" << 16 << ": " <<
        // indices.HyperLogLog<16>();
    };

    thrill::Run([&](thrill::Context &ctx) { start_func(ctx); });
}
