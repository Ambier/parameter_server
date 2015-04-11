#pragma once
#include "system/message.h"
#include "proto/filter.pb.h"
#include "base/shared_array_inl.h"

namespace ps {

/**
 * \brief The interface of a filter
 */
class IFilter {
 public:
  IFilter() { }
  virtual ~IFilter() { }

  static IFilter* create(const Filter& conf);

  virtual void Encode(Message* msg) { }
  virtual void Decode(Message* msg) { }

  static Filter* Find(Filter::Type type, Message* msg) {
    return Find(type, &(msg->task));
  }
  static Filter* Find(Filter::Type type, Task* task);
};

} // namespace
