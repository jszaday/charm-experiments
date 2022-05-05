#include <warp.hpp>

using namespace warp;

bool running = true;
int receive_index, nIters = 128;

struct pingpong : public task {
  int index_;
  int it_;
  bool warmup_;
  double start_time_;

  pingpong(int index) : index_(index), it_(0), warmup_(true) {
    request(index, receive_index);
  }

  static void receive(std::unique_ptr<task>& obj,
                      std::unique_ptr<message>&& msg) {
    auto& self = static_cast<pingpong&>(*obj);
    auto peer = (self.index_ + 1) % 2;

    auto done = (++self.it_) >= nIters;
    if (done) {
      done = self.finish(MPI_Wtime() - self.start_time_);
    }

    send(peer % nRanks, peer, receive_index, std::move(msg));

    if (!done) {
      request(self.index_, receive_index);
    }
  }

  bool finish(double time) {
    this->it_ = 0;
    this->start_time_ = MPI_Wtime();

    if (!this->warmup_) {
      if (this->index_ == 0) {
        std::cout << rank << "> roundtrip time was "
                  << ((1e6 * time) / (double)nIters) << " us" << std::endl;
      }

      if (this->index_ == rank) {
        running = false;
      }

      return true;
    } else {
      return (this->warmup_ = false);
    }
  }
};

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nRanks);

  // signal MPI that we don't care about the order of messages
  MPI_Info info;
  MPI_Info_create(&info);
  MPI_Info_set(info, "mpi_assert_allow_overtaking", "true");
  MPI_Comm_set_info(MPI_COMM_WORLD, info);
  MPI_Info_free(&info);

  int msg_size;
  nIters = (argc >= 2) ? atoi(argv[1]) : 128;
  msg_size = (argc >= 3) ? atoi(argv[2]) : 1024;

  entry_table.emplace_back(&pingpong::receive, msg_size);
  receive_index = entry_table.size() - 1;

  if (rank == 0) {
    tasks.emplace_back(new pingpong(rank));
    if (nRanks == 1) {
      tasks.emplace_back(new pingpong(1));
    }
  } else if (rank == 1) {
    tasks.emplace_back(nullptr);  // dummy to move over one
    tasks.emplace_back(new pingpong(rank));
  } else {
    running = false;
  }

  if (rank == 0) {
    std::cout << rank << "> pingpong with " << nIters << " iterations and "
              << msg_size << " B payload" << std::endl;
    send((rank + 1) % nRanks, 1, receive_index, message::allocate(msg_size));
  }

  while (running) {
    __run_once();
  }

  return MPI_Finalize();
}