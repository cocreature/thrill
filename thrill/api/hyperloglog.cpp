#include <thrill/api/hyperloglog.hpp>

namespace thrill {
namespace api {
int binarySearch(double rawEstimate, const std::vector<double> &estimatedData) {
    int length = estimatedData.size();

    int middle = length / 2;
    int lower = 0;
    int upper = length - 1;

    while (upper - lower > 1) {
        if (rawEstimate < estimatedData[middle]) {
            upper = middle - 1;
        } else {
            lower = middle;
        }
        middle = (upper + lower) / 2;
    }

    return lower;
}

double knearestNeighbor(int k, int index, double estimate,
                        const std::vector<double> &bias,
                        const std::vector<double> &estimateData) {
    double sum = 0;
    int estimateDataLength = estimateData.size();

    int lowerIndex = index;
    int upperIndex = index + 1;
    int neighbors = 0;
    while (neighbors < k) {
        double distLower;
        if (lowerIndex >= 0) {
            distLower = std::abs(estimate - estimateData[lowerIndex]);
        } else {
            distLower = std::numeric_limits<double>::infinity();
        }

        double distUpper;
        if (upperIndex < estimateDataLength) {
            distUpper = std::abs(estimateData[upperIndex] - estimate);
        } else {
            distUpper = std::numeric_limits<double>::infinity();
        }

        if (distLower <= distUpper) {
            sum += bias[lowerIndex];
            lowerIndex--;
        } else {
            sum += bias[upperIndex];
            upperIndex++;
        }
        neighbors++;
    }
    return sum / neighbors;
}
}

std::vector<uint8_t> encodeSparseList(const std::vector<uint32_t> &sparseList) {
    if (sparseList.empty()) {
        return {};
    }
    assert(std::is_sorted(sparseList.begin(), sparseList.end()));
    std::vector<uint8_t> sparseListBuffer;
    sparseListBuffer.reserve(sparseList.size());
    VectorWriter writer(sparseListBuffer);
    auto it = sparseList.begin();
    uint32_t prevVal = *it++;
    writer.PutVarint32(prevVal);
    for (; it != sparseList.end(); ++it) {
        writer.PutVarint32(*it - prevVal);
        prevVal = *it;
    }
    return sparseListBuffer;
}

std::vector<uint32_t>
decodeSparseList(const std::vector<uint8_t> &sparseListBuffer) {
    if (sparseListBuffer.empty()) {
        return {};
    }
    std::vector<uint32_t> sparseList;
    sparseList.reserve(sparseListBuffer.size());
    VectorReader reader(sparseListBuffer);
    sparseList.emplace_back(reader.GetVarint32());
    while (!reader.reachedEnd()) {
        sparseList.emplace_back(sparseList.back() + reader.GetVarint32());
    }
    return sparseList;
}
}
