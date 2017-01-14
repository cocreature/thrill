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
}
