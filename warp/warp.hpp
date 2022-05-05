#ifndef WARP_HPP
#define WARP_HPP

#include <mpi.h>

#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

namespace warp {

MPI_Aint tag_ub, obj_bits, ep_bits;

int __encode(std::size_t obj, std::size_t ep) {
  int tag = (obj << ep_bits) | ep;
  tag = (tag > tag_ub) ? -1 : tag;
  return tag;
}

struct message {
  std::size_t obj;
  std::size_t ep;
  std::size_t size;

  inline void* data(void) const;

  int tag(void) const { return __encode(this->obj, this->ep); }

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
std::size_t obj_id_lb, obj_id_ub;
std::vector<entry_info> entry_table;
std::vector<std::unique_ptr<task>> tasks;

using request_map_t = std::map<MPI_Request, std::unique_ptr<message>>;

request_map_t receives, sends;
std::queue<std::pair<std::size_t, std::size_t>> probes;

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
  MPI_Irecv(msg.get(), msg->total_size(), MPI_CHAR, MPI_ANY_SOURCE, msg->tag(),
            MPI_COMM_WORLD, &req);
  receives.emplace(req, std::move(msg));
}

// internal, receive a message into an allocated buffer
void __request(std::size_t obj, std::size_t ep, std::size_t size) {
  auto msg = message::allocate(size);
  msg->obj = obj;
  msg->ep = ep;
  request(std::move(msg));
}

// (async) receive a message to the obj's ep
void request(std::size_t obj, std::size_t ep) {
  auto& entry = entry_table[ep];
  auto& size = entry.size;
  if (size.has_value()) {
    __request(obj, ep, *size);
  } else {
    probes.emplace(obj, ep);
  }
}

void send(int rank, std::size_t obj, std::size_t ep,
          std::unique_ptr<message>&& msg) {
  msg->obj = obj;
  msg->ep = ep;
  MPI_Request req;
  MPI_Isend(msg.get(), msg->total_size(), MPI_CHAR, rank, msg->tag(),
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
    auto& [obj, ep] = probes.front();
    int flag, tag = __encode(obj, ep);
    // check if the probe is ready
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &flag, &status);
    // if so...
    if (flag) {
      // make the request
      int count;
      MPI_Get_count(&status, MPI_CHAR, &count);
      __request(obj, ep, (std::size_t)count);
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

  {
    int flag;
    MPI_Aint* tag_ub_ptr;
    err &= MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &tag_ub_ptr, &flag);
    tag_ub = *tag_ub_ptr;
  }

  auto tag_bits = (MPI_Aint)log2((double)tag_ub + 1);
  ep_bits = tag_bits / 2;
  obj_bits = tag_bits - ep_bits;

  auto obj_id_max = 0b1 << obj_bits;
  auto obj_id_per_rank = obj_id_max / nRanks;

  obj_id_lb = obj_id_per_rank * rank;
  obj_id_ub = (obj_id_per_rank * (rank + 1)) - 1;

  return (err == MPI_SUCCESS);
}
}  // namespace warp

#endif
