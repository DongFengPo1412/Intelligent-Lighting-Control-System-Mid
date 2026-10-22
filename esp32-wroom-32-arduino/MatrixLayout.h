#pragma once

// One 16x16 panel, viewed from the front. DIN is at the top-left.
// Even rows run left-to-right; odd rows run right-to-left.
namespace MatrixLayout {
constexpr int kWidth = 16;
constexpr int kHeight = 16;
constexpr int kPixelCount = kWidth * kHeight;

constexpr int PixelIndex(int x, int y) {
    return (x < 0 || x >= kWidth || y < 0 || y >= kHeight)
        ? -1
        : y * kWidth + ((y & 1) ? kWidth - 1 - x : x);
}
}  // namespace MatrixLayout
