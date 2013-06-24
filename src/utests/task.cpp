#include "../task.hpp"
#include <cstdio>

namespace cube {
static s32 somenumber = 1024*1024;

struct simpletask : public task {
  simpletask(void) : task("simple", somenumber, 1), x(0) {}
  void run(u32 elt) { x++; }
  atomic x;
};
struct simpletaskhiprio : public task {
  simpletaskhiprio(void) : task("simple", somenumber, 1, 0, task::HI_PRIO), x(0) {}
  void run(u32 elt) { x++; }
  atomic x;
};

#define CHECK(COND) do {\
  if (!(COND)) {\
    fprintf(stderr, "error with %s in function %s", #COND, __FUNCTION__);\
    exit(EXIT_FAILURE);\
  }\
} while (0)

void testsimpletask(void) {
  ref<simpletask> job = NEWE(simpletask);
  job->scheduled();
  job->wait();
  CHECK(job->x == somenumber);
}
void testsimpletaskhiprio(void) {
  ref<simpletaskhiprio> job = NEWE(simpletaskhiprio);
  job->scheduled();
  job->wait();
  CHECK(job->x == somenumber);
}
void testsimpletaskboth(void) {
  ref<simpletask> job0 = NEWE(simpletask);
  ref<simpletaskhiprio> job1 = NEWE(simpletaskhiprio);
  job0->scheduled();
  job1->scheduled();
  job0->wait();
  job1->wait();
  CHECK(job0->x == somenumber);
  CHECK(job1->x == somenumber);
}

struct simpletaskwait : public task {
  simpletaskwait(bool waitable) : task("simple", somenumber, waitable?1:0), x(0) {}
  void run(u32 elt) { x++; }
  atomic x;
};
void testsimpledep(void) {
  const s32 tasknum = 16;
  ref<simpletaskwait> jobs[tasknum];
  loopi(tasknum) jobs[i] = NEW(simpletaskwait, i==tasknum-1);
  loopi(tasknum-1) jobs[i]->starts(*jobs[i+1]);
  loopi(tasknum) jobs[i]->scheduled();
  jobs[tasknum-1]->wait();
  loopi(tasknum) CHECK(jobs[i]->x == somenumber);
}
void testsimpledepwithend(void) {
  const s32 tasknum = 16;
  ref<simpletaskwait> jobsstart[tasknum];
  ref<simpletaskwait> jobsend[2*tasknum];

  loopi(tasknum) jobsstart[i] = NEW(simpletaskwait, i==tasknum-1);
  loopi(tasknum) jobsend[i] = NEW(simpletaskwait, false);

  loopi(tasknum-1) jobsstart[i]->starts(*jobsstart[i+1]);
  loopi(tasknum) jobsend[i]->ends(*jobsstart[i]);
  loopi(tasknum) jobsend[i]->scheduled();
  loopi(tasknum) jobsstart[i]->scheduled();
  jobsstart[tasknum-1]->wait();
  loopi(tasknum) CHECK(jobsstart[i]->x == somenumber);
  loopi(tasknum) CHECK(jobsend[i]->x == somenumber);
}

int main(void) {
  const u32 threadnum = 1;
  tasking::init(&threadnum,1);
  testsimpletask();
  testsimpletaskhiprio();
  testsimpletaskboth();
  testsimpledep();
  testsimpledepwithend();
  tasking::clean();
  return 0;
}
#undef CHECK

} // namespace cube

int main(void) { return cube::main(); }

