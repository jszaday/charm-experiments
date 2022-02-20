#ifndef __HYPERCOMM_WORKGROUP_HPP__
#define __HYPERCOMM_WORKGROUP_HPP__

#include <pup.h>
#include <pup_stl.h>

namespace hypercomm {
struct task_id {
  int host_;
  std::uint32_t task_;

  bool operator==(const task_id &other) const {
    return (this->host_ == other.host_) && (this->task_ == other.task_);
  }
};

struct task_id_hasher_ {
  std::size_t operator()(const task_id &tid) const {
    static_assert(sizeof(std::size_t) == sizeof(task_id),
                  "sizes do not match!");
    auto &cast = reinterpret_cast<const std::size_t &>(tid);
    return std::hash<std::size_t>()(cast);
  }
};

using task_kind_t = std::uint32_t;
using continuation_id_t = std::uint32_t;
} // namespace hypercomm

PUPbytes(hypercomm::task_id);

#include <hypercomm/backstage_pass.hpp>
#include <hypercomm/workgroup.decl.h>

namespace hypercomm {
struct task_base_ {
  bool active_;
  task_kind_t kind_;
  continuation_id_t continuation_;

  task_base_(task_kind_t kind) : kind_(kind), active_(true) {}

  void pup_base_(PUP::er &p) {
    CkAssert(active_);

    p | this->continuation_;
  }
};
} // namespace hypercomm

namespace PUP {
template <> struct ptr_helper<hypercomm::task_base_, false>;
};

namespace hypercomm {
struct task_message : public CMessage_task_message {
  task_id tid;
  task_kind_t kind;
  char *data;

  static void *alloc(int msgnum, size_t sz, int *sizes, int pb,
                     GroupDepNum groupDepNum) {
    auto start = ALIGN_DEFAULT(sz);
    auto total_size = start + sizes[0];
    auto *msg = CkAllocMsg(msgnum, total_size, pb, groupDepNum);
    static_cast<task_message *>(msg)->data = (char *)msg + start;
    return msg;
  }

  static void *pack(void *raw) {
    auto *msg = static_cast<task_message *>(raw);
    msg->data = msg->data - (std::uintptr_t)msg;
    return raw;
  }

  static void *unpack(void *raw) {
    auto *msg = static_cast<task_message *>(raw);
    msg->data = (char *)msg + (std::uintptr_t)msg->data;
    return raw;
  }
};

struct task_payload {
  std::unique_ptr<CkMessage> src;
  std::size_t len;
  char *data;

  task_payload(task_message *msg)
      : src(msg), len(UsrToEnv(msg)->getTotalsize() - sizeof(task_message)),
        data(msg->data) {}

  task_payload(CkReductionMsg *msg, CkReduction::tupleElement *elt)
      : src(msg), len(elt->dataSize), data((char *)elt->data) {}

  task_payload(PUP::reconstruct) {}

  void pup(PUP::er &p) {
    if (p.isUnpacking()) {
      void *msg;
      CkPupMessage(p, &msg);
      this->src.reset((CkMessage *)msg);
      p | this->len;
      std::uintptr_t offset;
      p | offset;
      this->data = (char *)msg + offset;
    } else {
      void *msg = this->src.get();
      CkPupMessage(p, &msg);
      CkAssert(msg == this->src.get());
      p | this->len;
      auto offset = (std::uintptr_t)this->data - (std::uintptr_t)msg;
      p | offset;
    }
  }
};

template <typename T> struct task : public task_base_ {
  static task_kind_t kind_;

  using continuation_fn_t = void (T::*)(task_payload &&);

  task(void) : task_base_(kind_) {}

  template <continuation_fn_t Fn> void suspend(void);

  inline void terminate(void) { this->active_ = false; }

  template <typename Data>
  void all_reduce(Data &data, CkReduction::reducerType type);

  template <typename Data>
  void reduce(Data &data, CkReduction::reducerType type, int index);

  template <typename Data>
  void reduce(Data &data, CkReduction::reducerType type, const CkCallback &cb);

  template <typename... Args>
  void send(int index, Args&&... args);

  template <typename... Args>
  void broadcast(Args&&... args);

  inline int index(void) const {
    return static_cast<ArrayElement1D *>(CkActiveObj())->thisIndex;
  }
};

using task_creator_t = task_base_ *(*)(task_payload &&);
using task_puper_t = void (*)(PUP::er &, task_base_ *&);
using continuation_t = void (*)(task_base_ *, task_payload &&);

struct task_record_ {
  task_creator_t creator;
  task_puper_t puper;

  task_record_(task_creator_t creator_, task_puper_t puper_)
      : creator(creator_), puper(puper_) {}
};

using task_records_t = std::vector<task_record_>;

std::vector<continuation_t> &get_continuations_(void) {
  static std::vector<continuation_t> continuations;
  return continuations;
}

template <typename T> struct task_creation_helper_ {
  static task_base_ *create(task_payload &&payload) {
    return new T(std::move(payload));
  }
};

template <typename T> struct task_puping_helper_ {
  static void pup(PUP::er &p, task_base_ *&task) {
    if (p.isUnpacking()) {
      task = new T(PUP::reconstruct{});
    }

    task->pup_base_(p);

    ((T *)task)->pup(p);
  }
};

task_records_t &get_task_records_(void) {
  static task_records_t records;
  return records;
}

task_creator_t get_task_creator_(task_kind_t kind) {
  auto &records = get_task_records_();
  return records[kind].creator;
}

task_puper_t get_task_puper_(task_kind_t kind) {
  auto &records = get_task_records_();
  return records[kind].puper;
}

continuation_t get_continuation_(continuation_id_t id) {
  auto &continuations = get_continuations_();
  return continuations[id];
}

template <typename T> task_kind_t register_task_(void) {
  auto &records = get_task_records_();
  auto kind = records.size();
  records.emplace_back(&task_creation_helper_<T>::create,
                       &task_puping_helper_<T>::pup);
  return (task_kind_t)kind;
}

template <continuation_t Fn> continuation_id_t register_continuation_(void) {
  auto &continuations = get_continuations_();
  auto id = continuations.size();
  continuations.emplace_back(Fn);
  return (continuation_id_t)id;
}

template <typename T> task_kind_t task<T>::kind_ = register_task_<T>();

struct workgroup : public CBase_workgroup {
  template <typename T> friend struct task;

  workgroup(void) : last_task_(0) {}
  workgroup(CkMigrateMessage *) {}

  void create(task_message *);
  void resume(task_message *);
  void resume(CkReductionMsg *);
  bool resume(task_id, task_payload &);
  void pup(PUP::er &p);

  task_id generate_id(void) {
    return task_id{.host_ = thisIndex, .task_ = this->last_task_++};
  }

private:
  template <typename T>
  using task_map = std::unordered_map<task_id, T, task_id_hasher_>;

  task_id active_;
  std::uint32_t last_task_;
  task_map<std::unique_ptr<task_base_>> tasks_;
  task_map<std::deque<task_payload>> buffers_;

  void buffer_(task_id tid, task_payload &&payload);
  void flush_buffers_(task_id tid);
};

void workgroup::create(task_message *msg) {
  auto creator = get_task_creator_(msg->kind);
  auto tid = msg->tid;
  // record the active task
  this->active_ = tid;
  // then construct the task
  auto ins = this->tasks_.emplace(tid, creator(task_payload(msg)));
  CkAssertMsg(ins.second, "task creation unsucessful!");
  this->flush_buffers_(tid);
}

void workgroup::flush_buffers_(task_id tid) {
  auto search = this->buffers_.find(tid);
  if ((search == std::end(this->buffers_)) || search->second.empty()) {
    return;
  }
  auto &buffer = search->second;
  auto &payload = buffer.front();
  if (this->resume(tid, payload)) {
    buffer.pop_front();

    this->flush_buffers_(tid);
  }
}

void workgroup::resume(task_message *msg) {
  task_payload payload(msg);
  if (!this->resume(msg->tid, payload)) {
    this->buffer_(msg->tid, std::move(payload));
  }
}

void workgroup::resume(CkReductionMsg *msg) {
  int nReductions;
  CkReduction::tupleElement *results;
  msg->toTuple(&results, &nReductions);
  CkAssert(nReductions == 2);
  task_payload payload(msg, &results[1]);
  auto *set = (CkReduction::setElement *)results[0].data;
  CkAssert(set->next() == nullptr);
  auto &tid = *((task_id *)set->data);
  if (!this->resume(tid, payload)) {
    this->buffer_(tid, std::move(payload));
  }
}

bool workgroup::resume(task_id tid, task_payload &payload) {
  auto search = this->tasks_.find(tid);
  if (search == std::end(this->tasks_)) {
    return false;
  } else {
    auto &task = search->second;
    auto continuation = get_continuation_(task->continuation_);
    // set the active task
    this->active_ = tid;
    // invoke the continuation
    continuation(task.get(), std::move(payload));
    // then cleanup if the task shutdown
    if (!task->active_) {
      this->tasks_.erase(search);
    }
    return true;
  }
}

void workgroup::buffer_(task_id tid, task_payload &&payload) {
  auto search = this->buffers_.find(tid);
  if (search == std::end(this->buffers_)) {
    search = this->buffers_.emplace().first;
  }
  search->second.emplace_back(std::move(payload));
}

void workgroup::pup(PUP::er &p) {
  p | this->last_task_;
  p | this->tasks_;
  p | this->buffers_;
}

using workgroup_proxy = CProxy_workgroup;

template <typename... Args>
task_message* pack(Args &&... args) {
  auto tuple = std::forward_as_tuple(args...);

  PUP::sizer s;
  s | tuple;

  auto size = (int)s.size();
  auto *msg = new (&size) task_message;

  PUP::toMem p(msg->data);
  p | tuple;
  CkAssert(size == p.size());

  return msg;
}

template <typename T, typename... Args>
task_id launch(const workgroup_proxy &group, Args &&...args) {
  auto *local = group.ckLocalBranch();
  auto *elems =
      local ? &(local->*detail::get(detail::backstage_pass())) : nullptr;
  CkAssert(elems && !elems->empty());

  auto* msg = pack(std::forward<Args>(args)...);
  auto tid = ((workgroup *)elems->front())->generate_id();

  msg->tid = tid;
  msg->kind = task<T>::kind_;

  const_cast<workgroup_proxy &>(group).create(msg);

  return tid;
}

template <typename T, typename task<T>::continuation_fn_t Fn>
struct continuation_helper_ {
  static continuation_id_t id_;

  static void resume(task_base_ *task, task_payload &&payload) {
    (((T *)task)->*Fn)(std::move(payload));
  }
};

template <typename T, typename task<T>::continuation_fn_t Fn>
continuation_id_t continuation_helper_<T, Fn>::id_ =
    register_continuation_<&continuation_helper_<T, Fn>::resume>();

template <typename T>
template <typename task<T>::continuation_fn_t Fn>
void task<T>::suspend(void) {
  this->continuation_ = continuation_helper_<T, Fn>::id_;
}

template <typename T>
template <typename Data>
void task<T>::all_reduce(Data &data, CkReduction::reducerType type) {
  auto *host = static_cast<ArrayElement *>(CkActiveObj());
  this->reduce(data, type,
               CkCallback(CkIndex_workgroup::resume((CkReductionMsg *)nullptr),
                          host->ckGetArrayID()));
}

template <typename T>
template <typename Data>
void task<T>::reduce(Data &data, CkReduction::reducerType type, int index) {
  auto *host = static_cast<ArrayElement *>(CkActiveObj());
  this->reduce(data, type,
               CkCallback(CkIndex_workgroup::resume((CkReductionMsg *)nullptr),
                          CkArrayIndex1D(index), host->ckGetArrayID()));
}

template <typename T>
template <typename Data>
void task<T>::reduce(Data &data, CkReduction::reducerType type,
                     const CkCallback &cb) {
  PUP::sizer s;
  s | data;

  auto size = s.size();
  char *buf = (char *)alloca(size);
  PUP::toMemBuf(data, buf, size);

  auto *host = (workgroup *)CkActiveObj();
  auto &tid = host->active_;

  CkReduction::tupleElement redn[] = {
      CkReduction::tupleElement(sizeof(task_id), &tid, CkReduction::set),
      CkReduction::tupleElement(size, buf, type)};

  auto *msg = CkReductionMsg::buildFromTuple(redn, 2);
  msg->setCallback(cb);

  host->contribute(msg);
}

template <typename T>
template <typename... Args>
void task<T>::send(int index, Args &&... args) {
  auto *host = (workgroup*)CkActiveObj();
  auto *msg = pack(std::forward<Args>(args)...);
  msg->tid = host->active_;
  host->thisProxy[index].resume(msg);
}

template <typename T>
template <typename... Args>
void task<T>::broadcast(Args &&... args) {
  auto *host = (workgroup*)CkActiveObj();
  auto *msg = pack(std::forward<Args>(args)...);
  msg->tid = host->active_;
  host->thisProxy.resume(msg);
}
} // namespace hypercomm

namespace PUP {
template <> struct ptr_helper<hypercomm::task_base_, false> {
  inline void operator()(PUP::er &p, hypercomm::task_base_ *&t) const {
    using namespace hypercomm;
    auto kind = p.isUnpacking() ? 0 : t->kind_;
    p | kind;
    auto puper = get_task_puper_(kind);
    puper(p, t);
  }
};
}; // namespace PUP

#endif
