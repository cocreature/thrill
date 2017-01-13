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

#include <iostream>
#include <limits.h>
#include <thrill/api/all_reduce.hpp>
#include <thrill/common/functional.hpp>

namespace thrill {
namespace api {

static const size_t NUMBER_OF_REGISTERS = 54815;
using Registers = std::vector<uint32_t>;

template <typename ValueType>
class HyperLogLogNode final : public ActionResultNode<Registers> {
    static constexpr bool debug = false;
    using Super = ActionResultNode<Registers>;
    using Super::context_;

  public:
    template <typename ParentDIA>
    HyperLogLogNode(const ParentDIA &parent, const char *label)
        : Super(parent.ctx(), label, {parent.id()}, {parent.node()}),
          registers(NUMBER_OF_REGISTERS, 0.0) {
        auto pre_op_fn = [this](const ValueType &input) { PreOp(input); };

        auto lop_chain = parent.stack().push(pre_op_fn).fold();
        parent.node()->AddChild(this, lop_chain);
    }
    void PreOp(const ValueType &) {}
    void Execute() final {
        registers = context_.net.AllReduce(
            registers, [](auto regs1, auto) { return regs1; });
    }
    const Registers &result() const final { return registers; }

  private:
    Registers registers;
};

template <typename ValueType, typename Stack>
double DIA<ValueType, Stack>::HyperLogLog() const {
    assert(IsValid());
    auto node =
        common::MakeCounting<HyperLogLogNode<ValueType>>(*this, "HyperLogLog");
    node->RunScope();
    return 0.0;
}

} // namespace api
} // namespace thrill

#endif // !THRILL_API_HYPERLOGLOG_HEADER
