#include <mpi.h>

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

struct message {
  std::size_t obj;
  std::size_t ep;

  void* data(void) const { return ((std::byte*)this) + sizeof(message); }

  static std::unique_ptr<message> allocate(std::size_t size) {
    return std::unique_ptr<message>(
        (message*)(::operator new(sizeof(message) + size)));
  }
};

struct task {
  virtual ~task() = default;
};

struct entry_info {
  using entry_fn_t = void (*)(const std::unique_ptr<task>&,
                              std::unique_ptr<message>&&);

  entry_fn_t fn;
  std::optional<std::size_t> size;

  entry_info(entry_fn_t fn_, std::optional<std::size_t>&& size_)
      : fn(fn_), size(size_) {}
};

int rank, nRanks;
std::vector<entry_info> entry_table;
std::vector<std::unique_ptr<task>> tasks;
std::map<MPI_Request, std::unique_ptr<message>> sends;
std::map<MPI_Request, std::unique_ptr<message>> receives;
std::queue<std::pair<std::size_t, std::size_t>> probes;

int make_tag(std::size_t obj, std::size_t ep) { return (int)(obj + ep); }

void __request(int tag, std::size_t obj, std::size_t ep, std::size_t size) {
  auto msg = message::allocate(size);
  MPI_Request req;
  MPI_Irecv(msg.get(), sizeof(message) + size, MPI_CHAR, MPI_ANY_SOURCE, tag,
            MPI_COMM_WORLD, &req);
  receives.emplace(req, std::move(msg));
}

void request(std::size_t obj, std::size_t ep) {
  auto& entry = entry_table[ep];
  auto& size = entry.size;
  if (size.has_value()) {
    auto tag = make_tag(obj, ep);
    __request(tag, obj, ep, *size);
  } else {
    probes.emplace(obj, ep);
  }
}

void send(int rank, std::size_t obj, std::size_t ep, std::size_t size,
          std::unique_ptr<message>&& msg) {
  msg->obj = obj;
  msg->ep = ep;
  MPI_Request req;
  MPI_Isend(msg.get(), sizeof(message) + size, MPI_CHAR, rank,
            make_tag(obj, ep), MPI_COMM_WORLD, &req);
  sends.emplace(req, std::move(msg));
}

bool running = true;

void ping(const std::unique_ptr<task>&, std::unique_ptr<message>&&) {
  std::cout << rank << "> received ping" << std::endl;
  running = false;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nRanks);

  entry_table.emplace_back(&ping, std::nullopt);

  int lastRank = nRanks - 1;
  if ((rank == 0) || (rank == lastRank)) {
    auto msg_size = 1024;

    send((rank == 0) * lastRank, 0, 0, msg_size, message::allocate(msg_size));

    request(0, 0);
  } else {
    running = false;
  }

  while (running) {
    for (auto it = receives.begin(); it != receives.end();) {
      int flag;
      auto& [req, msg] = *(it);
      MPI_Test(const_cast<MPI_Request*>(&req), &flag, MPI_STATUS_IGNORE);
      if (flag) {
        auto& entry = entry_table[msg->ep];
        (entry.fn)(tasks[msg->obj], std::move(msg));
        it = receives.erase(it);
      } else {
        it++;
      }
    }

    for (auto it = sends.begin(); it != sends.end();) {
      int flag;
      auto& [req, msg] = *(it);
      MPI_Test(const_cast<MPI_Request*>(&req), &flag, MPI_STATUS_IGNORE);
      if (flag) {
        it = sends.erase(it);
      } else {
        it++;
      }
    }

    while (!probes.empty()) {
      auto& [obj, ep] = probes.front();
      auto tag = make_tag(obj, ep);
      int flag;

      MPI_Status status;
      MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &flag, &status);

      if (flag) {
        int count;
        MPI_Get_count(&status, MPI_CHAR, &count);
        __request(tag, obj, ep, (std::size_t)count);
        probes.pop();
      } else {
        break;
      }
    }
  }

  return MPI_Finalize();
}