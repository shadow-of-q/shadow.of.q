#include "task.hpp"
#include <SDL/SDL_thread.h>

namespace cube {
namespace tasking {

// track state of the task
enum {
  NEW = 0,
  SCHEDULED = 1,
  RUNNING = 2,
  DONE = 3
};

// a set of threads subscribes this queue and share the work in the list of the
// tasks. threads terminate when "terminatethreads" become true
struct queue {
  queue(u32 threadnum);
  ~queue(void);
  void append(task*);
  void terminate(task*);
  static int threadfunc(void*);
  SDL_cond *cond;
  SDL_mutex *mutex;
  vector<SDL_Thread*> threads;
  intrusive_list<task> readylist;
  volatile bool terminatethreads;
};
static vector<queue*> queues;

void queue::append(task *job) {
  ASSERT(job->owner == this && job->tostart == 0);
  SDL_LockMutex(job->owner->mutex);
    if (job->policy & task::HI_PRIO)
      job->owner->readylist.push_front(job);
    else
      job->owner->readylist.push_back(job);
  SDL_UnlockMutex(job->owner->mutex);
}

void queue::terminate(task *job) {
  ASSERT(job->owner == this && job->toend == 0);

  // run the finish function
  job->finish();
  storerelease(&job->state, u16(DONE));

  // go over all tasks that depend on us
  loopi(job->tasktostartnum)
    if (--job->taskstostart[i]->tostart == 0)
      append(job->taskstostart[i]);
  loopi(job->tasktoendnum)
    if (--job->taskstoend[i]->toend == 0)
      terminate(job->taskstoend[i]);

  // if no more waiters, we can safely free all dependencies since we are done
  if (job->waiternum == 0)
    loopi(job->depnum) job->deps[i]->release();
  job->release();
}

int queue::threadfunc(void *data) {
  queue *q = (queue*) data;
  for (;;) {
    SDL_LockMutex(q->mutex);
    while (q->readylist.empty() && !q->terminatethreads)
      SDL_CondWait(q->cond, q->mutex);
    if (q->terminatethreads) {
      SDL_UnlockMutex(q->mutex);
      break;
    }
    const auto job = q->readylist.front();
    // if unfair, we run all elements until there is nothing else to do in this
    // job. we do not care if a hi-prio job just arrived
    if (job->policy & task::UNFAIR) {
      job->acquire();
      SDL_UnlockMutex(q->mutex);
      for (;;) {
        const auto elt = --job->elemnum;
        if (elt >= 0) {
          job->run(elt);
          if (--job->toend == 0) q->terminate(job);
        }
        if (elt == 0) {
          SDL_LockMutex(q->mutex);
            q->readylist.pop_front();
          SDL_UnlockMutex(q->mutex);
        }
        if (elt <= 0) break;
      }
      job->release();
    }
    // if fair, we run once and go back in the queue to possibly run something
    // with hi-prio that just arrived
    else {
      const auto elt = --job->elemnum;
      if (elt == 0) q->readylist.pop_front();
      SDL_UnlockMutex(q->mutex);
      job->run(elt);
      if (--job->toend == 0) q->terminate(job);
    }
  }

  return 0;
}

queue::queue(u32 threadnum) : terminatethreads(false) {
  mutex = SDL_CreateMutex();
  cond = SDL_CreateCond();
  loopi(s32(threadnum)) threads.add(SDL_CreateThread(threadfunc, this));
}

queue::~queue(void) {
  terminatethreads = true;
  loopv(threads) SDL_WaitThread(threads[i], NULL);
  SDL_DestroyMutex(mutex);
  SDL_DestroyCond(cond);
}

void init(const u32 *queueinfo, u32 n) {
  queues.pad(n);
  loopi(s32(n)) queues[i] = NEW(queue, queueinfo[i]);
}

void clean(void) {
  loopv(queues) SAFE_DELETE(queues[i]);
  queues.setsize(0);
}
} // namespace tasking

task::task(const char *name, u32 n, u32 waiternum, u32 queue, u16 policy) :
  owner(tasking::queues[queue]), name(name), elemnum(n), tostart(0), toend(n),
  depnum(0), waiternum(waiternum), tasktostartnum(0), tasktoendnum(0),
  policy(policy), state(tasking::NEW)
{}

void task::run(int elt) {}
void task::finish(void) {}

void task::scheduled(void) {
  ASSERT(state == NEW);
  storerelease(&state, u16(tasking::SCHEDULED));
  acquire();
  if (--tostart) {
    storerelease(&state, u16(tasking::RUNNING));
    owner->append(this);
  }
}

void task::wait(void) {
  ASSERT(>state >= tasking::SCHEDULED && waiters > 0);

  // lift all starting dependencies
  while (tostart)
    loopi(depnum)
      if (deps[i]->toend)
        deps[i]->wait();

  // execute the run function
  for (;;) {
    const auto elt = --elemnum;
    if (elt <= 0) run(elt);
    if (elt == 0) {
      SDL_LockMutex(owner->mutex);
        owner->readylist.pop_front();
      SDL_UnlockMutex(owner->mutex);
      if (--toend) owner->terminate(this);
    }
    if (elt <= 0) break;
  }

  // execute all ending dependencies
  while (toend)
    loopi(depnum)
      if (deps[i]->toend)
        deps[i]->wait();

  // finished and no more waiters, we can safely all dependencies
  if (--waiternum)
    loopi(depnum) deps[i]->release();
}

task &task::starts(task *other) {
  ASSERT(state == NEW && other->state == NEW);
  const s32 startindex = tasktostartnum++;
  const s32 depindex = other->depnum++;
  ASSERT(startindex < MAXSTART && depindex < MAXDEP);
  taskstostart[startindex] = other;
  other->deps[depindex] = this;
  acquire();
  other->acquire();
  other->tostart++;
  return *this;
}

task &task::ends(task *other) {
  ASSERT(state == NEW && other->state < DONE);
  const s32 endindex = tasktoendnum++;
  const s32 depindex = other->depnum++;
  ASSERT(endindex < MAXEND && depindex < MAXDEP);
  taskstoend[endindex] = other;
  other->deps[depindex] = this;
  acquire();
  other->acquire();
  other->toend++;
  return *this;
}
} // namespace cube

