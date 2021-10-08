#include "ipc_lfq.hh"

const char* q0_name_ = "queue_0_";
const char* q1_name_ = "queue_1_";
constexpr auto num_msgs_ = 16;
using queue_type_ = ipc_lfq<int, num_msgs_>;

int main(int argc, char** argv) {
  assert(argc >= 2);
  auto procNum = atoi(argv[1]);

  auto* q0 = queue_type_::open(q0_name_);
  auto* q1 = queue_type_::open(q1_name_);

  auto* mine = (procNum == 0) ? q0 : q1;
  auto* theirs = (procNum == 0) ? q1 : q0;

  printf("%d> process started!\n", procNum);

  for (auto it = 0; it < num_msgs_; it++) {
    printf("%d> sending messages for it %d.\n", procNum, it);
    for (std::intptr_t i = 1; i <= num_msgs_; i++) {
      while (!theirs->enqueue(i))
        ;
    }

    printf("%d> waiting for messages for it %d.\n", procNum, it);
    for (auto i = 1; i <= num_msgs_; i++) {
      int j = 0;
      while (!mine->dequeue(&j))
        ;
      if (i != j) {
        printf("%d != %d\n", i, j);
        assert(false);
      }
    }
  }

  queue_type_::close(q0_name_, q0);
  queue_type_::close(q1_name_, q1);

  printf("%d> all tests passed!\n", procNum);

  return 0;
}
