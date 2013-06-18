#pragma once
#include "tools.hpp"
#include "stl.hpp"

namespace cube {
namespace task {

// interface for all jobs to run in the system
class job : public noncopyable, public intrusive_list_node, public refcount {
public:
  job(const char *jobname, u32 elemnum);
  void starts(job *other);
  void ends(job *other);
  virtual job *run(int elem) = 0;
private:
  vector<job*> jobtostart; // all the jobs that depend on this one
  vector<job*> jobtoend;   // all the jobs that wait for us to finish
  const char *name;        // debug facility mostly
  atomic tostart;          // mbz before starting
  atomic toend;            // mbz before ending
  atomic elemnum;          // number of items still to run in the set
  struct queue *owner;     // where the job needs to run when ready
  bool front;              // if true, push in front of the queue (high prio)
};

// init the tasking system with n queues. each queue has a number of threads
void init(const u32 *queueinfo, u32 n);
// shutdown the tasking system
void clean(void);
// append a job to run in the given queue
void run(job *j, u32 queue = 0, bool front = false);
// wait for the given job to complete (from main thread only)
void waitfor(job *j);
// run one job (from main thread only)
void runsomething(void);

} // namespace task
} // namespace cube

