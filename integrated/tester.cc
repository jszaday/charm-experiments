#include "pool.hh"
#include <time.h>

int my_node_ = 0;

int CmiMyNode(void) {
  return my_node_;
}

std::map<int, ipc_pool*> pools_;

ipc_pool *pool_for(const int& pe) {
  auto search = pools_.find(pe);
  if (search == std::end(pools_)) {
    if (pe == CmiMyNode()) {
      auto ins = pools_.emplace(pe, new ipc_pool(pe));
      assert(ins.second);
      search = ins.first;
    } else {
      return nullptr;
    }
  }
  return search->second;
}

int main(int argc, char** argv) {
  if (argc >= 2) {
    my_node_ = atoi(argv[1]);
  }

  auto *pool = pool_for(CmiMyNode());
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
