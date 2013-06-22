#pragma once
#include "tools.hpp"
#include "stl.hpp"

// simple tasking system that support waitable task with input / output
// dependencies
namespace cube {

namespace tasking {
  void init(const u32 *queueinfo, u32 n);
  void clean(void);
  struct queue;
};

class CACHE_LINE_ALIGNED task : public noncopyable, public intrusive_list_node, public refcount {
public:
  task(const char *taskname, u32 n=1, u32 waiternum=0, u32 queue=0, u16 policy=LO_PRIO|FAIR);
  task &starts(task*);
  task &ends(task*);
  void wait(void);
  void scheduled(void);
  virtual void run(int elt);
  virtual void finish(void);
  static const u32 LO_PRIO = 0;
  static const u32 HI_PRIO = 1;
  static const u32 FAIR    = 0;
  static const u32 UNFAIR  = 2;
private:
  friend struct tasking::queue;
  static const u32 MAXSTART = 4;
  static const u32 MAXEND   = 4;
  static const u32 MAXDEP   = 8;
  task *taskstostart[MAXSTART];// all the tasks that depend on this one
  task *taskstoend[MAXEND];    // all the tasks that wait for us to finish
  task *deps[MAXDEP];          // all the tasks we depend on to finish or start
  tasking::queue *const owner; // where the task needs to run when ready
  const char *name;            // debug facility mostly
  atomic elemnum;              // number of items still to run in the set
  atomic tostart;              // mbz before starting
  atomic toend;                // mbz before ending
  atomic depnum;               // all the task we depend to finish
  atomic waiternum;            // number of wait that still need to be done
  atomic tasktostartnum;       // number of tasks we need to start
  atomic tasktoendnum;         // number of tasks we need to end
  const u16 policy;            // if true, push in front of the queue (high prio)
  volatile u16 state;          // track task state
};
} // namespace cube

