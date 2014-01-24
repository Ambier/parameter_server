#pragma once

#include "util/common.h"
#include "util/mail.h"
#include "util/status.h"
#include "util/key.h"
#include "util/lock.h"
#include "util/blocking_queue.h"
#include "box/consistency.h"
#include "system/aggregator.h"
#include "system/shared_obj.h"

namespace PS {

class Postoffice;

static const bool kDelta = true;
static const bool kValue = false;
static const uid_t kServer = NodeGroup::kServers;
static const int kCurTime = -1;

// the base container without template. this class contains the information
// required by the engine
class Container : public SharedObj {
 public:
  explicit Container(const string& name);
  Container(const string& name, Key min_key, Key max_key);

  // initialization of container
  // init postoffice and postmaster, it they are not inited'
  // get the the local key_range of this container
  // input : the global key range of this container,
  // TODO we may get it from the master node
  void Init(KeyRange whole);
  // wait until this container is initialized
  virtual void WaitInited();
  // TODO replace bool by some meaningful class, say statisific informations
  // about sending and receiving
  typedef std::shared_future<bool> Future;
  // push and pull
  Status Push(const Header& h); //, Future* push_fut=NULL, Future* pull_fut=NULL);
  Status Pull(const Header& h); //, Future* pull_fut=NULL);

  // accessors
  const name_t& name() const { return name_; }
  // KeyRange key_range() { return key_range_; }

  // consistency
  void SetMaxDelay(int push, int pull) {
    max_push_delay_ = std::max(push, 0);
    max_pull_delay_ = std::max(pull, 0);
  }
  void SetMaxPushDelay(int delay) {
    max_push_delay_ = std::max(delay, 0);
  }
  void SetMaxPullDelay(int delay) {
    max_pull_delay_ = std::max(delay, 0);
  }

  // aggregator
  void SetAggregator(int node_group) {
    aggregator_.SetDefaultType(node_group);
  }

  // wait until all pull and push requests <= time are success, the default is cur_time
  void Wait(int time = kCurTime);

  // prepare data for communication
  // set key* in mail.flag, fill in keys and values
  virtual Status GetLocalData(Mail *mail) = 0;
  virtual Status MergeRemoteData(const Mail& mail) = 0;
  // virtual Status  //

  // increase the clock, and return the new time. it is thread safe
  int IncrClock() { ScopeLock lock(&mutex_); return ++cur_time_; }
  int Clock() const { return cur_time_; }

  // set callbacks
  void SetRecvFunc(Closure* callback) {recv_callback_ = callback;}
  void SetAggregatorFunc(Closure* callback) {aggregator_callback_ = callback;}
  void SetSendFunc(Closure* callback) {send_callback_ = callback;}

  // query about my node
  Node& my_node() { return postmaster_->addr_book()->my_node(); }

  // the short name, for debug use
  string SName() { return StrCat(my_node().ShortName(), ": "); }

  // this two functions are provided to postoffice, and usually run by the
  // postoffice threads
  // accept a mail from the postoffice
  virtual void Accept(const Mail& mail) {
    WaitInited();
    mails_received_.Put(mail);
    if (mail.flag().type() & Header::REPLY) {
      int32 time = mail.flag().time();
      pull_aggregator_.Insert(mail);
      if (pull_aggregator_.Success(time, postmaster_->GetNodeGroup(name()))) {
        pull_pool_.Set(time, true);
        pull_aggregator_.Delete(time);
      }
      // LL << SName() << "accept: " << mail.flag().time();
    }
    if (my_node().is_server())
      ReadAll();
  }
  // notify the container if the mail is sent
  virtual void Notify(const Header& flag) {
    if (flag.type() & Header::PUSH) {
      // LL << SName() << "notify: " << flag.time();
      push_pool_.Set(flag.time(), true);
    }
    // LL << SName() << "notify: " << name() << " " <<  flag.DebugString();
  }
 protected:
  // process all mails in the receiving queue
  void ReadAll();
  void Reply(const Mail& from_other, const Mail& my_reply);
  bool AggregateSuccess(int32 time) {
    return aggregator_.Success(time, postmaster_->GetNodeGroup(name()));
  }

  name_t name_;

  Aggregator aggregator_;

  // the key_range this containers has. for a server, usually it is a segment of
  // the whole key range, while a client usually has the whole key, but only
  // need to access a subset of <key,value> pairs
  KeyRange key_range_;

  // the current logic time, increase by 1 for every call to Push or Pull
  int cur_time_;
  Mutex mutex_;

  BlockingQueue<Mail> mails_received_;

  Postoffice *postoffice_;
  Postmaster *postmaster_;

  // call after any data are received
  Closure* recv_callback_;
  // call after data are aggregated
  Closure* aggregator_callback_;
  // call after data are send
  Closure* send_callback_;

  // the consistency model
  // Consistency<bool> consistency_;
  static const int kInfDelay = kint32max;
  int max_push_delay_;
  int max_pull_delay_;

  // TODO replace bool with some statistic information about sending and receiving
  FuturePool<bool> push_pool_;
  FuturePool<bool> pull_pool_;
  // store the receivers of pull request
  Aggregator pull_aggregator_;

  bool container_inited_;
};

} // namespace PS
