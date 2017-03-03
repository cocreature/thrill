#include <thrill/api/cache.hpp>
#include <thrill/api/dia.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/hyperloglog.hpp>
#include <thrill/api/size.hpp>
#include <thrill/common/logger.hpp>
#include <thrill/common/stats_timer.hpp>

#include <random>
#include <tuple>
#include <utility>
#include <vector>

// using thrill::DIARef;
using thrill::Context;

using namespace thrill; // NOLINT

int main() {

    auto start_func = [](std::ofstream &outStream, size_t sampleSize,
                         api::Context &ctx) {
        std::vector<size_t> data;
        static std::uniform_int_distribution<size_t> distribution(
            std::numeric_limits<size_t>::min(),
            std::numeric_limits<size_t>::max());
        static std::default_random_engine generator;
        if (ctx.my_rank() == 0) {
            data.resize(sampleSize);
            std::generate(data.begin(), data.end(),
                          []() { return distribution(generator); });
        }
        auto in = Distribute<size_t>(ctx, data, 0).Cache();
        // This should hopefully force evaluation
        thrill::common::StatsTimerStart timer;
        double count = in.HyperLogLog<14>();
        timer.Stop();
        if (ctx.my_rank() == 0) {
            outStream << sampleSize << "," << timer.Microseconds() << "\n";
            std::sort(data.begin(), data.end());
            size_t exactCount =
                std::unique(data.begin(), data.end()) - data.begin();
            std::cout << "exactCount: " << exactCount
                      << ", approxCount: " << count << "\n";
        }
    };
    const std::vector<size_t> sampleSizes = {10,     100,     1000,    10000,
                                             100000, 1000000, 10000000};
    const size_t iterations = 50;
    std::ofstream outStream;
    outStream.open("hyperloglog_benchmark");
    outStream << "size,time\n";
    for (size_t sampleSize : sampleSizes) {
        for (size_t iter = 0; iter < iterations; ++iter) {
            api::Run([&start_func, &outStream, sampleSize](api::Context &ctx) {
                start_func(outStream, sampleSize, ctx);
            });
        }
    }
    outStream.close();
}

/******************************************************************************/
