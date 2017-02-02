#include <functional>
#include <iomanip>

#include <thrill/api/collapse.hpp>
#include <thrill/api/generate.hpp>
#include <thrill/api/hyperloglog.hpp>

using namespace thrill;

template <size_t p>
void evaluate(api::MemoryConfig &mem_config,
              const std::vector<size_t> &sampleSizes,
              const size_t iterationsPerSize, std::ofstream &outStream) {
    static std::uniform_int_distribution<size_t> distribution(
        std::numeric_limits<size_t>::min(), std::numeric_limits<size_t>::max());
    static std::default_random_engine generator;
    size_t progressStringLength = std::to_string(iterationsPerSize).size();
    size_t maxSampleStringLength =
        std::to_string(
            *std::max_element(sampleSizes.begin(), sampleSizes.end()))
            .size();
    for (auto sampleSize : sampleSizes) {
        for (size_t i = 0; i < iterationsPerSize; ++i) {
            api::RunLocalMock(mem_config, 1, 2, [&](Context &ctx) {
                if (ctx.my_rank() == 0) {
                    std::cerr
                        << "\r[" << std::setw(progressStringLength) << (i + 1)
                        << "/" << iterationsPerSize << "] "
                        << "precision: " << std::setw(2) << p
                        << ", sample size: " << std::setw(maxSampleStringLength)
                        << sampleSize;
                }
                std::vector<size_t> data;
                if (ctx.my_rank() == 0) {
                    data.resize(sampleSize);
                    std::generate(data.begin(), data.end(),
                                  []() { return distribution(generator); });
                }
                double hyperloglogCount =
                    Distribute<size_t>(ctx, data, 0).HyperLogLog<p>();
                if (ctx.my_rank() == 0) {
                    std::sort(data.begin(), data.end());
                    size_t uniqueCount =
                        std::unique(data.begin(), data.end()) - data.begin();
                    outStream << p << "," << sampleSize << "," << uniqueCount
                              << "," << hyperloglogCount << "\n";
                }
            });
        }
        std::cerr << "\n";
    }
}

void evaluateDifferentPrecisions(api::MemoryConfig &mem_config,
                                 const size_t iterationsPerSize) {
    std::cerr << "Precision comparison\n";

    std::ofstream outStream;
    outStream.open("precision_comparison");
    outStream
        << "hyperloglog_precision,samplesize,exact_count,hyperloglog_count\n";

    std::vector<size_t> sampleSizes{10, 100, 1000, 10000, 100000};

    evaluate<4>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<5>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<6>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<7>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<8>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<9>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<11>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<12>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<13>(mem_config, sampleSizes, iterationsPerSize, outStream);
    evaluate<14>(mem_config, sampleSizes, iterationsPerSize, outStream);

    outStream.close();
}

void evaluateSinglePrecision(api::MemoryConfig &mem_config,
                             const size_t iterationsPerSize) {
    std::cerr << "Single precision\n";

    std::ofstream outStream;
    outStream.open("single_precision");
    outStream
        << "hyperloglog_precision,samplesize,exact_count,hyperloglog_count\n";

    int stepSize = 400;
    std::vector<size_t> sampleSizes(100000 / stepSize);
    size_t n = 0;
    std::generate(sampleSizes.begin(), sampleSizes.end(), [&] {
        n += stepSize;
        return n;
    });
    evaluate<14>(mem_config, sampleSizes, iterationsPerSize, outStream);

    outStream.close();
}

int main() {
    const size_t iterationsPerSize = 100;

    // This is used to hide the verbose output
    api::MemoryConfig mem_config;
    mem_config.setup(4 * 1024 * 1024 * 1024llu);
    mem_config.verbose_ = false;

    evaluateSinglePrecision(mem_config, iterationsPerSize);
    evaluateDifferentPrecisions(mem_config, iterationsPerSize);
}
