/*******************************************************************************
 * thrill/net/group.cpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <thrill/net/group.hpp>
#include <thrill/net/manager.hpp>
#include <thrill/net/mock/group.hpp>

#if THRILL_HAVE_NET_TCP
#include <thrill/net/tcp/group.hpp>
#endif

#include <vector>

namespace thrill {
namespace net {

void RunLoopbackGroupTest(
    size_t num_hosts,
    const std::function<void(Group*)>& thread_function) {
#if THRILL_HAVE_NET_TCP
    // construct local tcp network mesh and run threads
    ExecuteGroupThreads(
        tcp::Group::ConstructLoopbackMesh(num_hosts),
        thread_function);
#else
    // construct mock network mesh and run threads
    ExecuteGroupThreads(
        mock::Group::ConstructLoopbackMesh(num_hosts),
        thread_function);
#endif
}

/******************************************************************************/
// Manager

void Manager::RunTask(const std::chrono::steady_clock::time_point& tp) {

    common::JsonLine line = logger_.line();
    line << "class" << "NetManager";

    unsigned long long elapsed
        = std::chrono::duration_cast<std::chrono::microseconds>(
        tp - tp_last_).count();

    size_t total_tx = 0, total_rx = 0;
    size_t prev_total_tx = 0, prev_total_rx = 0;

    for (size_t g = 0; g < kGroupCount; ++g) {
        Group& group = *groups_[g];

        size_t group_tx = 0, group_rx = 0;
        size_t prev_group_tx = 0, prev_group_rx = 0;
        std::vector<size_t> tx_per_host(group.num_hosts());
        std::vector<size_t> rx_per_host(group.num_hosts());

        for (size_t h = 0; h < group.num_hosts(); ++h) {
            if (h == group.my_host_rank()) continue;

            // this is a benign data race with the send methods -- we don't need
            // the up-to-date value. -tb
            size_t tx = group.connection(h).tx_bytes_;
            size_t rx = group.connection(h).rx_bytes_;
            size_t prev_tx = group.connection(h).prev_tx_bytes_;
            size_t prev_rx = group.connection(h).prev_rx_bytes_;

            group_tx += tx;
            prev_group_tx += prev_tx;
            group.connection(h).prev_tx_bytes_ = tx;

            group_rx += rx;
            prev_group_rx += prev_rx;
            group.connection(h).prev_rx_bytes_ = rx;

            tx_per_host[h] = tx;
            rx_per_host[h] = rx;
        }

        line.sub(g == 0 ? "flow" : g == 1 ? "data" : "???")
            << "tx_bytes" << group_tx
            << "rx_bytes" << group_rx
            << "tx_speed"
            << static_cast<double>(group_tx - prev_group_tx) / elapsed * 1e6
            << "rx_speed"
            << static_cast<double>(group_rx - prev_group_rx) / elapsed * 1e6
            << "tx_per_host" << tx_per_host
            << "rx_per_host" << rx_per_host;

        total_tx += group_tx;
        total_rx += group_rx;
        prev_total_tx += prev_group_tx;
        prev_total_rx += prev_group_rx;

        tp_last_ = tp;
    }

    // write out totals
    line
        << "tx_bytes" << total_tx
        << "rx_bytes" << total_rx
        << "tx_speed"
        << static_cast<double>(total_tx - prev_total_tx) / elapsed * 1e6
        << "rx_speed"
        << static_cast<double>(total_rx - prev_total_rx) / elapsed * 1e6;
}

} // namespace net
} // namespace thrill

/******************************************************************************/
