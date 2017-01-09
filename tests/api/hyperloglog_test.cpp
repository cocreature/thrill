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
        LOG << "executing start_func";
        auto indices = Generate(ctx, 1);
        double estimate = indices.HyperLogLog();
        LOG << "hyperlog result: " << estimate;
    };

    thrill::Run([&](thrill::Context &ctx) { start_func(ctx); });
}
