/*!
 * @file   ps.h
 * \brief  The parameter server interface
 */
#ifndef DMLC_PS_H_
#define DMLC_PS_H_
#include "./base.h"
#include "./blob.h"
namespace dmlc {
namespace ps {

/*! \brief The default type of a key */
typedef uint64_t K;

///////////////////////////////////////////////////////////////////////////////
///                              Worker node APIs                           ///
///////////////////////////////////////////////////////////////////////////////

/**
 * \brief The main function for a worker node
 *
 * All flags and their arguments (e.g. -logtostderr 1) has been parsed and removed
 * from argc and argv, but commandline arguments are remained such as data=my_data.txt
 */
int WorkerNodeMain(int argc, char *argv[]);

/**
 * \brief Options for Push and Pull
 */
struct SyncOpts {
  /**
   * \brief the timestamp of the depended requests. This request will be
   * processed by the parameter servers only after the depended requests have
   * been processed.
   */
  std::initializer_list<int> deps;
  /**
   * \brief the function will be executed after received the
   * response from the parameter server
   */
  std::function<void()> callback;
  /**
   * \brief zero-copy synchronization. Keys (and values) will not be copied to
   * reduce the communication delay. Therefore, it is the user's responsibility
   * to keep the keys and values unchanged until the request is finished, namely
   * Wait(ts) returns or the callback is called.
   */
  bool zero_copy = false;
};

/*!
 * \brief key-value cache for sending (receiving) key-value pairs to (from) servers
 *
 * @tparam V the type of value
 */
template<typename V>
class KVCache {
 public:
  /**
   * @param id the unique identity which is used to find the KVStore at the
   * parameter server. Negative IDs is preserved by system.
   */
  explicit KVCache(int id = 0);
  ~KVCache();

  /*!
   * \brief Pushes a list of key-value pairs into the parameter server
   *
   * It's a non-blocking call, which returns immediately once the message is
   * queued in the system's sending buffer. The actual push is finished only
   * after Wait(returned_timestamp) returns or the provided callback is called.
   *
   * Both keys and values will be copied, using the SArray version for zero-copy
   * pushing.
   *
   * Sample usage: assume we have two key-value pairs {1, (1.1, 1.2)}, {3,
   * (3.1,3.2)}, where the value is a 2-length float vector. We then can push these
   * two pairs into the parameter server:
   \code
     KVCache<float> cache(0);
     std::vector<K> keys = {1, 3};
     std::vector<float> vals = {1.1, 1.2, 3.1, 3.2};
     cache.Push(keys, vals);
   \endcode
   *
   * @param keys a list of keys
   * @param values a list of values, whose size should be an integer multiple
   * the key size
   *
   * @return the timestamp of this request.
   */
  int Push(const std::vector<K>& keys, const std::vector<V>& values,
           const SyncOpts& opts = SyncOpts()) {
    return Push(CBlob<K>(keys), CBlob<V>(values), opts);
  }

  /*!
   * \brief Pulls the values associated with the keys from the parameter server
   *
   * It's a non-blocking call, which returns immediately once the message is
   * queued in the system's sending buffer. The actual push is finished only
   * after Wait(returned_timestamp) returns or the provided callback is called.
   *
   * Keys will be copied, using the SArray version for zero-copy pushing.
   *
   * @param keys a list of keys
   * @param values the buffer for the pulled values, which should be pre-allocated
   *
   * Sample usage: again assume each key is associated with a 2-length float
   * vector value. We then can pull the newest value from the parameter server:
   \code
     KVCache<float> cache(0);
     std::vector<K> keys = {1, 3};
     std::vector<float> vals(4);
     cache.Pull(keys, &vals);
   \endcode
   * @return the timestamp of this request
   */
  int Pull(const std::vector<K>& keys, std::vector<V>* values,
           const SyncOpts& opts = SyncOpts()) {
    return Pull(CBlob<K>(keys), Blob<V>(*values), opts);
  }

  /*!
   * \brief Waits until a request has been finished
   *
   * Sample usage:
   \code
     int ts = cache.Pull(keys, &vals);
     Wait(ts);
     // now vals is ready for use
   \endcode
   */
  void Wait(int timestamp);

  /*! \brief Blob style Push and Pull */

  int Push(CBlob<K> keys, CBlob<V> values, const SyncOpts& opts = SyncOpts());
  int Pull(CBlob<K> keys, Blob<V> values, const SyncOpts& opts = SyncOpts());

  /*! \brief More advanced Push and Pull by using shared blob */

  int Push(const SBlob<K>& keys, const SBlob<V>& values,
           const SyncOpts& opts = SyncOpts());
  int Pull(const SBlob<K>& keys, SBlob<V>* values,
           const SyncOpts& opts = SyncOpts());

  /*!
   * \brief Increases the clock by delta
   */
  void IncrClock(int delta = 1);
 private:
};

///////////////////////////////////////////////////////////////////////////////
///                             Server node APIs                            ///
///////////////////////////////////////////////////////////////////////////////

/**
 * \brief The main function for a server node
 *
 * All flags and their arguments (e.g. -logtostderr 1) has been parsed and removed
 * from argc and argv, but commandline arguments are remained such as data=my_data.txt
 */
int CreateServerNode(int argc, char *argv[]);

/**
 * \brief An example of user-defineable handle. See more handle examples in
 * ps_server_handle.h
 * \tparam V the value type
 */
template <typename V>
class IHandle {
 public:
  IHandle() { }
  virtual ~IHandle() { }

  /**
   * \brief Handle PUSH requests from worker nodes
   *
   * @param recv_keys the keys received from a worker node
   * @param recv_vals the corresponding values received from the worker node
   * @param my_vals the corresponding local values
   */
  inline void HandlePush(CBlob<V> recv_keys, CBlob<V> recv_vals,
                         Blob<V> my_vals) {
    LOG(FATAL) << "implement this function";
  }
  /**
   * \brief Handle PUSH requests from worker nod
   *
   * @param recv_keys the keys received from a worker node
   * @param my_vals the corresponding local values
   * @param sent_vals the corresponding values will send to the worker node
   */
  inline void HandlePull(CBlob<V> recv_keys, CBlob<V> my_vals,
                         Blob<V> send_vals) {
    LOG(FATAL) << "implement this function";
  }

  /**
   * \brief Initialize local values
   */
  inline void HandleInit(CBlob<V> keys, Blob<V> vals) {
    LOG(FATAL) << "implement this function";
  }
};


#define DYNAMIC_LEN -1

/*!
 * \brief key-value store for server nodes
 *
 * @tparam V the value type
 * @Handle User-defined handles
 * @tparam val_len the length of a value (= val_len * sizeof(V)) that stored in
 * local. It could be a dynamic length DYNAMIC_LEN
 * @tparam sync_val_len the length of value will be synchronized
 */
template <typename V, typename Handle = IHandle<V>,
          int val_len = 1, int sync_val_len = 1>
class KVStore {
 public:
  /**
   * \brief Process key-value pairs in online or batch style
   *
   * - ONLINE: individual key-value pairs received from workers are feed into
   *   user-defined writer/reader one by one.
   *
   * - BATCH: all key-value pairs received from a worker in a Push/Pull request
   *   are feed into writer/reader togeter
   *
   * Implementation & Performance
   *
   * - ONLINE: use unordered_map or other equivalence data structure to store KV
   *   pairs. It is suitable when new keys appears during running, such as
   *   SGD/online learning algorithms. However, both read and write could be 5x
   *   slower comparing to BATCH
   *
   * - BATCH: use array to store KV pairs. Suitable for the keys set is fixed at
   *   the beginning, such as batch algorithm. Both read and write are fast, but
   *
   */
  enum Type { ONLINE, BATCH };

  /**
   * @param type which affects how key-value pairs are feed into updater and
   *  initializer, see comments below
   * @param id the unique identity. Negative IDs is preserved by system.
   */
  KVStore(int id = 0, Type type = ONLINE) { }
  ~KVStore() { }

  Handle& handle() { return handle_; }
  void Run() { }
 private:
  Handle handle_;
};


///////////////////////////////////////////////////////////////////////////////
///                            Scheduler Node APIs                          ///
///////////////////////////////////////////////////////////////////////////////
// TODO

///////////////////////////////////////////////////////////////////////////////
///                            More Advanced APIs                           ///
///////////////////////////////////////////////////////////////////////////////
/// ///

/*! \brief Return true if this node is a worker node. */
bool IsWorkerNode() { return true; }

/*! \brief Return true if this node is a server node. */
bool IsServerNode() { return true; }

/*! \brief Return true if this node is a scheduler node. */
bool IsSchedulerNode() {return true;  }

/*! \brief The global unique string ID of this node */
std::string MyNodeID() { return std::string(); }


}  // namespace ps
}  // namespace dmlc

/// implementation

#endif  /* DMLC_PS_H_ */

#include "../../src/ps/ps-inl.h"
