/* charmlite completion detection demo
 *
 * author: j. szaday <szaday2@illinois.edu>
 */

#include "cmk.decl.hh"

// a chare that uses an int for its index
struct completion : public cmk::chare<completion, int> {
  using detection_message =
      cmk::data_message<std::tuple<cmk::collective_index_t, cmk::callback>>;

  struct status {
    detection_message* msg;
    std::int64_t lcount;
    bool complete;

    status(detection_message* msg_) : msg(msg_), lcount(0), complete(false) {}
  };

  struct count {
    cmk::collective_index_t detector;
    cmk::collective_index_t target;
    std::int64_t gcount;

    count(cmk::collective_index_t detector_, cmk::collective_index_t target_,
          std::int64_t gcount_)
        : detector(detector_), target(target_), gcount(gcount_) {}

    count& operator+=(const count& other) {
      this->gcount += other.gcount;
      return *this;
    }
  };

  cmk::collective_map<status> statii;

  completion(void) = default;

  using count_message = cmk::data_message<count>;

  status& get_status(cmk::collective_index_t idx,
                     detection_message* msg = nullptr) {
    auto find = this->statii.find(idx);
    if (find == std::end(this->statii)) {
      find = this->statii.emplace(idx, msg).first;
    } else if (msg) {
      find->second.msg = msg;
    }
    return find->second;
  }

  void start_detection(detection_message* msg) {
    auto& val = msg->value();
    auto& idx = std::get<0>(val);
    auto& status = this->get_status(idx, msg);
    CmiEnforce(msg != nullptr);
    if (status.complete) {
      std::get<1>(val).send(cmk::all, msg);
    } else {
      auto* count = new count_message(this->collective(), idx, status.lcount);
      cmk::all_reduce<cmk::add<typename count_message::type>, receive_count_>(
          count);
    }
  }

  void produce(cmk::collective_index_t idx, std::int64_t n = 1) {
    this->get_status(idx).lcount += n;
  }

  void consume(cmk::collective_index_t idx, std::int64_t n = 1) {
    this->produce(idx, -n);
  }

  static void receive_count_(cmk::message* msg) {
    auto& gcount = static_cast<count_message*>(msg)->value();
    auto* self = cmk::lookup(gcount.detector)->lookup<completion>(CmiMyPe());
    auto& status = self->get_status(gcount.target);
    status.complete = (gcount.gcount == 0);
    self->start_detection(status.msg);
    cmk::message::free(msg);
  }
};

struct test : cmk::chare<test, int> {
  cmk::group_proxy<completion> detector;
  bool detection_started_;

  test(cmk::data_message<cmk::group_proxy<completion>>* msg)
      : detector(msg->value()), detection_started_(false) {}

  void produce(cmk::message* msg) {
    auto* local = detector.local_branch();
    if (local == nullptr) {
      auto elt = this->element_proxy();
      elt.send<cmk::message, &test::produce>(msg);
    } else {
      CmiPrintf("%d> producing %d value(s)...\n", CmiMyPe(), CmiNumPes());
      detector.local_branch()->produce(this->collective(), CmiNumPes());
      cmk::group_proxy<test> col(this->collective());
      col.broadcast<cmk::message, &test::consume>(msg);
    }
  }

  void consume(cmk::message* msg) {
    auto* local = detector.local_branch();
    if (local == nullptr) {
      auto elt = this->element_proxy();
      // put the message back to await local branch creation
      elt.send<cmk::message, &test::consume>(msg);
    } else {
      CmiPrintf("%d> consuming a value...\n", CmiMyPe());

      local->consume(this->collective());

      // start completion detection if we haven't already
      if (!detection_started_ && (this->index() == 0)) {
        detector.broadcast<completion::detection_message,
                           &completion::start_detection>(
            new completion::detection_message(
                this->collective(), cmk::callback::construct<cmk::exit>()));

        detection_started_ = true;
      }

      delete msg;
    }
  }
};

int main(int argc, char** argv) {
  cmk::initialize(argc, argv);
  if (CmiMyNode() == 0) {
    // establish the groups
    auto detector = cmk::group_proxy<completion>::construct();
    auto* dm = new cmk::data_message<decltype(detector)>(detector);
    auto arr = cmk::group_proxy<test>::construct(dm);
    // send each message a "produce" message
    // to start the process
    for (auto i = 0; i < CmiNumPes(); i++) {
      auto elt = arr[i];
      elt.send<cmk::message, &test::produce>(new cmk::message);
    }
  }
  cmk::finalize();
  return 0;
}

// registers user-defined types with the rts
#include "cmk.def.hh"
