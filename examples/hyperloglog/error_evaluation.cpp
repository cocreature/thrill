#include <functional>
#include <iomanip>

#include <thrill/api/collapse.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/hyperloglog.hpp>

using namespace thrill;

template <size_t p>
void evaluate(Context &ctx, const std::vector<size_t> &sampleSizes,
              const size_t iterationsPerSize, std::ofstream &outStream) {
    static std::uniform_int_distribution<size_t> distribution(
        std::numeric_limits<size_t>::min(), std::numeric_limits<size_t>::max());
    static std::default_random_engine generator;
    size_t progressStringLength = std::to_string(iterationsPerSize).size();
    size_t maxSampleStringLength =
        std::to_string(
            *std::max_element(sampleSizes.begin(), sampleSizes.end()))
            .size();
    DIA<size_t> distributedData;
    for (auto sampleSize : sampleSizes) {
        for (size_t i = 0; i < iterationsPerSize; ++i) {
            if (ctx.my_rank() == 0) {
                std::cerr << "\r[" << std::setw(progressStringLength) << (i + 1)
                          << "/" << iterationsPerSize << "] "
                          << "precision: " << std::setw(2) << p
                          << ", sample size: "
                          << std::setw(maxSampleStringLength) << sampleSize;
            }
            std::vector<size_t> data;
            if (ctx.my_rank() == 0) {
                data.resize(sampleSize);
                std::generate(data.begin(), data.end(),
                              []() { return distribution(generator); });
            }
            distributedData = Distribute<size_t>(ctx, data, 0).Collapse();
            double hyperloglogCount = distributedData.HyperLogLog<p>();

            if (ctx.my_rank() == 0) {
                std::sort(data.begin(), data.end());
                size_t uniqueCount =
                    std::unique(data.begin(), data.end()) - data.begin();
                outStream << p << "," << sampleSize << "," << uniqueCount << ","
                          << hyperloglogCount << "\n";
            }
        }
        if (ctx.my_rank() == 0) {
            std::cerr << "\n";
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Please pass the path to the output file\n";
        exit(1);
    }

    std::ofstream outStream;
    outStream.open(argv[1]);
    outStream
        << "hyperloglog_precision,samplesize,exact_count,hyperloglog_count\n";

    const std::vector<size_t> sampleSizes = {10,    100,    1000,
                                             10000, 100000, 1000000};
    const size_t iterationsPerSize = 100;

    // This is used to hide the verbose output
    api::MemoryConfig mem_config;
    mem_config.setup(4 * 1024 * 1024 * 1024llu);
    mem_config.verbose_ = false;

    // If I don’t start separate instances, I run out of memory. There is
    // probably some way to prevent that from happening but I don’t know how.
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<4>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<5>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<6>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<7>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<8>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<9>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<10>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<11>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<12>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<13>(ctx, sampleSizes, iterationsPerSize, outStream);
    });
    api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
        evaluate<14>(ctx, sampleSizes, iterationsPerSize, outStream);
    });

    outStream.close();
}
