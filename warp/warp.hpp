#ifndef WARP_HPP
#define WARP_HPP

#include <mpi.h>

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>
#include <cmath>

namespace warp {

struct message {
  std::size_t obj;
  std::size_t ep;
  std::size_t size;
  int tag;

  inline void set_properties(std::size_t obj, std::size_t ep, int tag) {
    this->obj = obj;
    this->ep = ep;
    this->tag = tag;
  }

  inline void* data(void) const;

  static std::unique_ptr<message> allocate(std::size_t size) {
    auto* msg = (message*)(::operator new(sizeof(message) + size));
    msg->size = size;
    return std::unique_ptr<message>(msg);
  }

  std::size_t total_size(void) const { return this->size + sizeof(message); }
};

inline void* message::data(void) const {
  return ((std::byte*)this) + sizeof(message);
}

struct task {
  virtual ~task() = default;
};

struct entry_info {
  using entry_fn_t = void (*)(std::unique_ptr<task>&,
                              std::unique_ptr<message>&&);

  entry_fn_t fn;
  std::optional<std::size_t> size;

  entry_info(entry_fn_t fn_, std::optional<std::size_t>&& size_)
      : fn(fn_), size(size_) {}
};

int rank, nRanks;
std::vector<entry_info> entry_table;
std::vector<std::unique_ptr<task>> tasks;

using request_map_t = std::map<MPI_Request, std::unique_ptr<message>>;

request_map_t receives, sends;
std::queue<std::tuple<std::size_t, std::size_t, int>> probes;

template <typename Fn>
void __foreach_request(request_map_t& map, const Fn& fn) {
  for (auto it = map.begin(); it != map.end();) {
    int flag;
    auto& [req, msg] = *(it);
    MPI_Test(const_cast<MPI_Request*>(&req), &flag, MPI_STATUS_IGNORE);
    if (flag) {
      fn(std::move(msg));
      it = map.erase(it);
    } else {
      it++;
    }
  }
}

// receive a message directly into a preallocated buffer
void request(std::unique_ptr<message>&& msg) {
  MPI_Request req;
  MPI_Irecv(msg.get(), msg->total_size(), MPI_CHAR, MPI_ANY_SOURCE, msg->tag,
            MPI_COMM_WORLD, &req);
  receives.emplace(req, std::move(msg));
}

// internal, receive a message into an allocated buffer
void __request(std::size_t obj, std::size_t ep, std::size_t size, int tag) {
  auto msg = message::allocate(size);
  msg->set_properties(obj, ep, tag);
  request(std::move(msg));
}

// (async) receive a message to the obj's ep
void request(std::size_t obj, std::size_t ep, int tag) {
  auto& entry = entry_table[ep];
  auto& size = entry.size;
  if (size.has_value()) {
    __request(obj, ep, *size, tag);
  } else {
    probes.emplace(obj, ep, tag);
  }
}

void send(int rank, std::size_t obj, std::size_t ep, int tag,
          std::unique_ptr<message>&& msg) {
  MPI_Request req;
  msg->set_properties(obj, ep, tag);
  MPI_Isend(msg.get(), msg->total_size(), MPI_CHAR, rank, msg->tag,
            MPI_COMM_WORLD, &req);
  sends.emplace(req, std::move(msg));
}

inline void __run_once(void) {
  // iterate through all sends (free'ing done ones)
  __foreach_request(sends, [](auto&&) {});
  // iterate through all receives (triggering their EP)
  __foreach_request(receives, [](auto&& msg) {
    auto& entry = entry_table[msg->ep];
    (entry.fn)(tasks[msg->obj], std::move(msg));
  });
  // iterate through the probe queue...
  while (!probes.empty()) {
    int flag;
    auto& [obj, ep, tag] = probes.front();
    // check if the probe is ready
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &flag, &status);
    // if so...
    if (flag) {
      // make the request
      int count;
      MPI_Get_count(&status, MPI_CHAR, &count);
      __request(obj, ep, (std::size_t)count - sizeof(message), tag);
      probes.pop();
    } else {
      break;
    }
  }
}

bool initialize(int& argc, char**& argv) {
  auto err = MPI_SUCCESS;
  MPI_Init(&argc, &argv);
  err &= MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  err &= MPI_Comm_size(MPI_COMM_WORLD, &nRanks);

  // signal MPI that we don't care about the order of messages
  MPI_Info info;
  err &= MPI_Info_create(&info);
  err &= MPI_Info_set(info, "mpi_assert_allow_overtaking", "true");
  err &= MPI_Comm_set_info(MPI_COMM_WORLD, info);
  err &= MPI_Info_free(&info);

  return (err == MPI_SUCCESS);
}
}  // namespace warp

#endif
