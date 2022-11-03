#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

using namespace std;

int cpu_clock = 0;
double total_wait_time = 0.0;
double total_turnaround_time = 0.0;
int total_num_processes = 0;

struct Process {
  int pid;
  int burst;
  int burst_backup;
  int arrival;
  int finish;

  int age;

  int last_run_end;

  Process(int pid, int burst, int arrival)
      : pid(pid),
        burst(burst),
        burst_backup(burst),
        arrival(arrival),
        finish(-1),
        age(0),
        last_run_end(arrival) {
    printf("Process %d: arrives @%9d\n", pid, arrival);
  }

  bool run_for(int slice) {
    // printf("Process %d: runs @%12d\n", pid, cpu_clock);
    burst -= slice;
    age = 0;
    last_run_end = cpu_clock + slice;

    if (burst <= 0) {  // process finish
      finish = cpu_clock + (burst + slice);
      printf("Process %d: finished @%8d\n", pid, finish);
      total_wait_time += finish - arrival - burst_backup;
      total_turnaround_time += finish - arrival;
      return true;
    }
    printf("Process %d: switched @%8d\n", pid, last_run_end);
    return false;
  }
};

struct ProcessQueue {
  enum class SchedulingMethod { RR, FCFS } scheduling_method;

  int time_quantum;
  bool is_age_enabled;
  int age_limit;

  queue<shared_ptr<Process>> process_queue;

  shared_ptr<ProcessQueue> upper_queue;
  shared_ptr<ProcessQueue> lower_queue;

  ProcessQueue(SchedulingMethod m,
               int tq,
               bool enable_age,
               int age_limit,
               shared_ptr<ProcessQueue> upper)
      : scheduling_method(m),
        time_quantum(tq),
        is_age_enabled(enable_age),
        age_limit(age_limit),
        upper_queue(upper),
        lower_queue(nullptr) {}

  bool is_empty() { return process_queue.empty(); }

  int run() {
    auto p = process_queue.front();
    process_queue.pop();

    if (scheduling_method == SchedulingMethod::RR) {
      if (p->run_for(time_quantum)) {  // process finish
        total_num_processes++;
        return p->finish - cpu_clock;
      }
      if (lower_queue) {
        lower_queue->process_queue.push(p);
      } else {
        process_queue.push(p);
      }
      return time_quantum;
    } else {  // FCFS
      auto burst = p->burst;
      p->run_for(burst);
      total_num_processes++;
      return burst;
    }
  }

  void age_all(int tq) {
    auto size = process_queue.size();
    while (size--) {
      auto p = process_queue.front();
      process_queue.pop();
      p->age += tq;
      if (p->age > age_limit && upper_queue) {
        upper_queue->process_queue.push(p);
      } else {
        process_queue.push(p);
      }
    }
  }

  void add_process(int pid, int burst, int arrival) {
    process_queue.push(make_shared<Process>(pid, burst, arrival));
  }

  int get_runtime() {
    if (scheduling_method == SchedulingMethod::RR)
      return time_quantum;
    return process_queue.front()->burst;
  }
};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <input filename>\n", argv[0]);
    exit(1);
  }

  fprintf(stderr, "Running MFQS ......\n");

  int num_queue;
  fprintf(stderr, "Enter number of queues: ");
  scanf("%d", &num_queue);

  int tq_top;
  fprintf(stderr, "Enter time quantum for top queue: ");
  scanf("%d", &tq_top);

  int age_int;
  fprintf(stderr, "Enter ageing interval: ");
  scanf("%d", &age_int);

  fprintf(stderr, "\n");

  vector<shared_ptr<ProcessQueue>> qs;
  shared_ptr<ProcessQueue> upper = nullptr;
  for (int i = 0; i < num_queue - 1; i++) {
    qs.push_back(make_shared<ProcessQueue>(ProcessQueue::SchedulingMethod::RR,
                                           tq_top, false, age_int, upper));
    tq_top *= 2;
    upper = qs.back();
  }
  qs.push_back(make_shared<ProcessQueue>(ProcessQueue::SchedulingMethod::FCFS,
                                         tq_top, true, age_int, upper));
  for (int i = 0; i < num_queue - 1; i++) {
    qs[i]->lower_queue = qs[i + 1];
  }

  // init queue
  ifstream in(argv[1]);
  string line;
  getline(in, line);  // header

  int pid, burst, arrival, _pri, _dline, _io;
  arrival = 0x7fffffff;
  while (true) {
    // check if there are processes to run
    int i;
    for (i = 0; i < num_queue; i++) {
      if (!qs[i]->is_empty())  // have process to schedule
      {
        int pid_cur = qs[i]->process_queue.front()->pid;
        printf("Process %d: runs @%12d\n", pid_cur, cpu_clock);

        int runtime = qs[i]->get_runtime();
        // add new processes
        while (cpu_clock <= arrival && arrival < cpu_clock + runtime) {
          qs[0]->add_process(pid, burst, arrival);
          if (in >> pid)
            in >> burst >> arrival >> _pri >> _dline >> _io;
          else
            break;
        }
        // run
        auto delta = qs[i]->run();
        qs[num_queue - 1]->age_all(delta);
        cpu_clock += delta;
        break;
      }
    }
    if (i == num_queue) {
      if (arrival != 0x7fffffff) {
        qs[0]->add_process(pid, burst, arrival);
      } else if (in >> pid >> burst >> arrival >> _pri >> _dline >> _io) {
        cpu_clock = arrival;
        qs[0]->add_process(pid, burst, arrival);
      } else {
        arrival = 0x7fffffff;
        break;
      }
      if (!(in >> pid >> burst >> arrival >> _pri >> _dline >> _io))
        arrival = 0x7fffffff;
    }
  }
  total_num_processes--;

  printf("Ave. wait time = %lf\t", total_wait_time / total_num_processes);
  printf("Ave. turnaround time = %lf\n",
         total_turnaround_time / total_num_processes);
  printf("Total processes scheduled = %d\n", total_num_processes);

  return 0;
}
