
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ratio>

int main(int argc, char** argv) {
  auto size = (argc >= 2) ? atoi(argv[1]) : (128 * 1024);
  auto nIters = 1024;
  auto nReps = 11;
  auto nSkip = (nReps / 2) + 1;

  auto* src = (char*)aligned_alloc(alignof(char), size);
  auto* dst = (char*)aligned_alloc(alignof(char), size);

  auto totalTime = 0.0;
  for (auto rep = 0; rep < (nReps + nSkip); rep++) {
    auto start = std::chrono::high_resolution_clock::now();
    for (auto it = 0; it < nIters; it++) {
      memcpy(dst, src, size);
      std::swap(dst, src);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> time(end - start);
    if (rep >= nSkip) {
      totalTime += time.count();
    }
  }

  auto avgTime = totalTime / nReps;
  std::cout << "average time to copy " << (size * nIters) << "B in " << size
            << "B chunks: " << avgTime << "us" << std::endl;
  std::cout << "average time to copy " << size << "B: " << avgTime / nIters
            << "us" << std::endl;

  free(src);
  free(dst);

  return 0;
}
