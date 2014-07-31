#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <chrono>
#include <fstream>
#include <iostream>

#include "caf/all.hpp"

#ifdef __APPLE__
# include <mach/mach.h>
# include <mach/message.h>
# include <mach/task_info.h>
# include <mach/kern_return.h>
#endif

using namespace std;
using namespace caf;

namespace { decltype(chrono::system_clock::now()) s_start; }

#ifdef __APPLE__
template <class OutStream>
bool print_vsize(OutStream& out, const string&, pid_t child) {
  task_t child_task;
  if (task_for_pid(mach_task_self(), child, &child_task) != KERN_SUCCESS) {
    return false;
  }
  task_basic_info_data_t basic_info;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  if (task_info(child_task, TASK_BASIC_INFO, (task_info_t) &basic_info, &count) != KERN_SUCCESS) {
    return false;
  }
  // type is mach_vm_size_t
  out << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - s_start).count()
      << " "
      << static_cast<unsigned long long>(basic_info.resident_size) << endl;
  return true;
}
#else
#endif

void watchdog(blocking_actor* self, pid_t child, int max_runtime) {
  self->delayed_send(self, chrono::seconds(max_runtime), atom("TimeIsUp"));
  self->receive(
    on(atom("TimeIsUp")) >> [=] {
      kill(child, 9);
    }
  );
}

void memrecord(blocking_actor* self, pid_t child, int poll_interval) {
  string fname = "/proc/";
  fname += std::to_string(child);
  fname += "/status";
  //self->delayed_send(self, chrono::milliseconds(50), atom("poll"));
  self->send(self, atom("poll"));
  self->receive_loop(
    on(atom("poll")) >> [&] {
      print_vsize(cout, fname, child);
      self->delayed_send(self, chrono::milliseconds(poll_interval), atom("poll"));
    }
  );
}

void usage() {
  cout << "usage: caf_run_bench USERID MAX_RUNTIME_IN_SEC MEM_POLL_INTERVAL_IN_MS PROGRAM ARGS..."
       << endl;
  exit(1);
}

int rd_int(const char* cstr) {
  try {
    return stoi(cstr);
  }
  catch (...) {
    usage();
  }
  return 42; // unreachable
}

int main(int argc, char** argv) {
  if (argc < 5) {
    usage();
  }
  int userid = rd_int(argv[1]);
  int max_runtime = rd_int(argv[2]);
  int poll_interval = rd_int(argv[3]);
  cout << "fork into " << argv[4] << endl;
  pid_t child_pid = fork();
  if (child_pid < 0) {
    throw std::logic_error("fork failed");
  }
  s_start = chrono::system_clock::now();
  if (child_pid == 0) {
    if (setuid(userid) != 0) {
      cerr << "could not set userid to " << userid << endl;
      exit(1);
    }
    // skip path to app, userid, max runtime, and poll interval
    auto first = argv + 4;
    auto last = first + argc;
    vector<char*> arr;
    copy(first, last, back_inserter(arr));
    arr.push_back(nullptr);
    execv(*first, arr.data());
    perror("execv");
    // should be unreachable
    throw std::logic_error("execv failed");
  }
  auto dog = spawn<detached + blocking_api>(watchdog, child_pid, max_runtime);
  auto rec = spawn<detached + blocking_api>(memrecord, child_pid, poll_interval);
  int child_exit_status = 0;
  wait(&child_exit_status);
  auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - s_start);
  anon_send_exit(dog, exit_reason::user_shutdown);
  anon_send_exit(rec, exit_reason::user_shutdown);
  cout << "exit status: " << child_exit_status << endl;
  cout << "program did run for " << duration.count() << "ms" << endl;
  await_all_actors_done();
  shutdown();
}
