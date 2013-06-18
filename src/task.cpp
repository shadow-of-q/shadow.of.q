#include "task.hpp"
#include <SDL/SDL_thread.h>

namespace cube {
namespace task {

job::job(const char *name, u32 n) : name(name), tostart(1), toend(1), elemnum(n) {
  this->refinc();
}

void job::starts(job *other) {
  other->tostart++;
  other->refinc();
  jobtostart.add(other);
}

void job::ends(job *other) {
  other->toend++;
  other->refinc();
  jobtoend.add(other);
}

// a set of threads subscribe this queue and share the work in the list of the
// jobs. threads terminate when "terminatethreads" become true
struct queue {
  queue(u32 threadnum);
  ~queue(void);
  void insert(job *job);
  static void threadfunc(void *data);
  SDL_cond *cond;
  SDL_mutex *mutex;
  vector<SDL_Thread*> threads;
  intrusive_list<job*> readylist;
  volatile bool terminatethreads;
};

queue::queue(u32 threadnum) : terminatethreads(false) {

}

queue::~queue(void) {

}

static vector<queue*> queues;
static s32 queuenum = 0;

void init(const u32 *queueinfo, u32 n) {
  queuenum = n;
  queues.pad(queuenum);
  loopi(queuenum) queues[i] = NEW(queue, queueinfo[i]);
}

void clean(void) {
  loopi(queuenum) SAFE_DELETE(queues[i]);
}

void run(job *j, u32 queue, bool front) {

}

void waitfor(job *j) {

}

void runsomething(void) {

}

} // namespace task
} // namespace cube

