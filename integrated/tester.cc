#include "pool.hh"
#include <time.h>

int main(int argc, char** argv) {
  auto pool = new ipc_pool(0);
  auto maxSize = 128;
  auto numBlocks = 8;

  for (auto i = 0; i < numBlocks; i++) {
    assert(pool->lput(malloc(maxSize), maxSize));
    assert(pool->lput(malloc(maxSize / 2), maxSize / 2));
  }

  srand(time(0));
  for (auto i = 0; i < numBlocks * 2;) {
    auto sz = (rand() % maxSize) + 1;
    auto* block = pool->ralloc(sz);
    printf("info> requesting block of size %d bytes... %s\n", sz, block ? "pass" : "fail");
    if (block != nullptr) {
      i++;
      free(block->ptr);
      delete block;
    }
  }

  assert(pool->ralloc(64) == nullptr);

  return 0;
}
