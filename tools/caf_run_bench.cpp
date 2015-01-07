#include <pwd.h>
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
bool print_rss(OutStream& out, string&, const string&, pid_t child) {
  task_t child_task;
  if (task_for_pid(mach_task_self(), child, &child_task) != KERN_SUCCESS) {
    return false;
  }
  task_basic_info_data_t basic_info;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  if (task_info(child_task, TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&basic_info), &count) != KERN_SUCCESS) {
    return false;
  }
  auto rss = static_cast<unsigned long long>(basic_info.resident_size);
  auto rss_kb = rss / 1024;
  // type is mach_vm_size_t
  out << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - s_start).count()
      << " "
      << rss_kb << endl;
  return true;
}
#elif defined(__linux__)
template <class OutStream>
bool print_rss(OutStream& out, string& line, const string& proc_file, pid_t) {
  ifstream statfile(proc_file);
  while (getline(statfile, line)) {
    if (line.compare(0, 6, "VmRSS:") == 0) {
      auto first = line.c_str() + 6; // skip "VmRSS:"
      auto rss = strtoll(first, NULL, 10);
      if (line.compare(line.size() - 2, 2, "kB") != 0) {
        cerr << "VmRSS is *NOT* in kB" << endl;
      }
      out << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - s_start).count()
          << " "
          << rss << endl;
      return true;
    }
  }
  return false;
}
#else
# error OS not supported
#endif

void watchdog(blocking_actor* self, int max_runtime) {
  pid_t child;
  self->receive(
    on(atom("go"), arg_match) >> [&](pid_t child_pid) {
      child = child_pid;
    }
  );
  self->delayed_send(self, chrono::seconds(max_runtime), atom("TimeIsUp"));
  self->receive(
    on(atom("TimeIsUp")) >> [=] {
      kill(child, 9);
    }
  );
}

void memrecord(blocking_actor* self, int poll_interval, std::ostream& out) {
  string fname = "/proc/";
  string line_buf;
  pid_t child;
  self->receive(
    on(atom("go"), arg_match) >> [&](pid_t child_pid) {
      child = child_pid;
    }
  );
  fname += std::to_string(child);
  fname += "/status";
  self->send(self, atom("poll"));
  self->receive_loop(
    on(atom("poll")) >> [&] {
      self->delayed_send(self, chrono::milliseconds(poll_interval), atom("poll"));
      print_rss(out , line_buf, fname, child);
    }
  );
}

void usage() {
  cout << "usage: caf_run_bench USERID MAX_RUNTIME_IN_SEC "
          "MEM_POLL_INTERVAL_IN_MS RUNTIME_OUT_FILE MEM_OUT_FILE "
          "PROGRAM ARGS..."
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
  if (argc < 7) {
    usage();
  }
  uid_t userid = static_cast<uid_t>(rd_int(argv[1]));
  int max_runtime = rd_int(argv[2]);
  int poll_interval = rd_int(argv[3]);
  string runtime_out_fname = argv[4];
  string mem_out_fname = argv[5];
  std::fstream runtime_out{runtime_out_fname, ios_base::out | ios_base::app};
  std::fstream mem_out{mem_out_fname, ios_base::out};
  std::ostringstream mem_out_buf;
  if (!runtime_out) {
    cerr << "unable to open file for runtime output: " << runtime_out_fname << endl;
    return 1;
  }
  if (!mem_out) {
    cerr << "unable to open file for memory output: " << mem_out_fname << endl;
    return 1;
  }
  // start background workers
  auto dog = spawn<detached + blocking_api>(watchdog, max_runtime);
  auto rec = spawn<detached + blocking_api>(memrecord, poll_interval,
                                            std::ref(mem_out_buf));
  cout << "fork into " << argv[6] << endl;
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
    // make sure $HOME is set properly (evaluated by Erlang)
    auto pw = getpwuid(userid);
    std::string env_cmd = "HOME=";
    env_cmd += pw->pw_dir;
    if (putenv(&env_cmd[0]) != 0) {
      cerr << "could net set HOME to " << pw->pw_dir << endl;
      exit(1);
    }
    // skip path to app, userid, max runtime, and poll interval
    auto first = argv + 6;
    auto last = first + argc;
    vector<char*> arr;
    copy(first, last, back_inserter(arr));
    arr.push_back(nullptr);
    execv(*first, arr.data());
    perror("execv");
    // should be unreachable
    throw std::logic_error("execv failed");
  }
  auto msg = make_message(atom("go"), child_pid);
  anon_send(dog, msg);
  anon_send(rec, msg);
  int child_exit_status = 0;
  wait(&child_exit_status);
  auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - s_start);
  anon_send_exit(dog, exit_reason::user_shutdown);
  anon_send_exit(rec, exit_reason::user_shutdown);
  cout << "exit status: " << child_exit_status << endl;
  cout << "program did run for " << duration.count() << "ms" << endl;
  await_all_actors_done();
  shutdown();
  if (child_exit_status == 0) {
    runtime_out << duration.count() << endl;
    mem_out << mem_out_buf.str() << flush;
  }
  return child_exit_status;
}
