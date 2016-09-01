/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <chrono>
#include <utility>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

namespace {

void usage() {
    cout << "Running in server mode:"                                    << endl
         << "  mode=server  "                                            << endl
         << "  --port=NUM       publishes an actor at port NUM"          << endl
         << "  -p NUM           alias for --port=NUM"                    << endl
         << endl
         << endl
         << "Running the benchmark:"                                     << endl
         << "  mode=benchmark run the benchmark, connect to any number"  << endl
         << "                 of given servers, use HOST:PORT syntax"    << endl
         << "  num_pings=NUM  run benchmark with NUM messages per node"  << endl
         << endl
         << "  example: mode=benchmark 192.168.9.1:1234 "
                                        "192.168.9.2:1234 "
                                        "--num_pings=100"                << endl
         << endl
         << endl
         << "Shutdown servers:"                                          << endl
         << "  mode=shutdown  shuts down any number of given servers"    << endl
         << endl
         << endl
         << "Miscellaneous:"                                             << endl
         << "  -h, --help       print this text and exit"                << endl
         << endl;
    exit(0);
}

class ping_actor : public event_based_actor {
public:
  ping_actor(actor_config& cfg, actor parent)
      : event_based_actor(cfg),
        parent_(move(parent)) {
    // nop
  }

  behavior make_behavior() override {
    return {
      on(atom("kickoff"), arg_match) >> [=](actor pong, uint32_t value) {
        send(pong, atom("ping"), value);
        become (
          on(atom("pong"), uint32_t(0)) >> [=] {
            send(parent_, atom("done"));
            quit();
          },
          on(atom("pong"), arg_match) >> [=](uint32_t x) {
            return make_tuple(atom("ping"), x - 1);
          },
          others >> [=] {
            cout << "ping_actor: unexpected: "
                 << to_string(current_message())
                 << endl;
          }
        );
      },
      others >> [=] {
          cout << "ping_actor: unexpected: "
               << to_string(current_message())
               << endl;
      }
    };
  }

private:
  actor parent_;
};

class server_actor : public event_based_actor {
public:
    typedef map<pair<string, uint16_t>, actor> pong_map;

    behavior make_behavior() override {
      trap_exit(true);
      return {
        on(atom("ping"), arg_match) >> [=](uint32_t value) {
          return make_tuple(atom("pong"), value);
        },
        on(atom("add_pong"), arg_match) >> [=](const string& host, uint16_t port) -> message {
          auto key = make_pair(host, port);
          auto i = m_pongs.find(key);
          if (i == m_pongs.end()) {
            try {
              auto p = system().middleman().remote_actor(host.c_str(), port);
              link_to(p);
              m_pongs.insert(make_pair(key, p));
            }
            catch (exception& e) {
              return make_message(atom("error"), e.what());
            }
          }
          return make_message(atom("ok"));
        },
        on(atom("kickoff"), arg_match) >> [=](uint32_t num_pings, actor buddy) {
          for (auto& kvp : m_pongs) {
            auto ping = spawn<ping_actor>(buddy);
            send(ping, atom("kickoff"), kvp.second, num_pings);
          }
        },
        on(atom("purge")) >> [=] {
            m_pongs.clear();
        },
        on<atom("EXIT"), uint32_t>() >> [=] {
          auto who = current_sender();
          auto i = find_if(m_pongs.begin(), m_pongs.end(),
                           [&](const pong_map::value_type& kvp) {
            return kvp.second == who;
          });
          if (i != m_pongs.end())
            m_pongs.erase(i);
        },
        on(atom("shutdown")) >> [=] {
          m_pongs.clear();
          quit();
        },
        others >> [=] {
          cout << "unexpected: " << to_string(current_message()) << endl;
        }
    };
  }

private:
  pong_map m_pongs;
};

template<typename Arg0>
void usage(Arg0&& arg0) {
  cout << forward<Arg0>(arg0) << endl << endl;
  usage();
}

template<typename Arg0, typename Arg1, typename... Args>
void usage(Arg0&& arg0, Arg1&& arg1, Args&&... args) {
  cout << forward<Arg0>(arg0);
  usage(forward<Arg1>(arg1), forward<Args>(args)...);
}

template<typename Iterator>
void server_mode(Iterator first, Iterator last) {
  string port_prefix = "--port=";
  // extracts port from a key-value pair
  auto kvp_port = [&](const string& str) -> maybe<int> {
    if (equal(port_prefix.begin(), port_prefix.end(), str.begin())) {
      return spro<int>(str.c_str() + port_prefix.size());
    }
    return {};
  };
  message_builder{first, last}.apply({
    (on(kvp_port) || on("-p", spro<int>)) >> [](int port) {
      if (port > 1024 && port < 65536)
        io::publish(spawn<server_actor>(), static_cast<uint16_t>(port));
      else
        usage("illegal port: ", port);
    },
    others >> [=] {
      if (first != last) usage("illegal argument: ", *first);
      else usage();
    }
  });
}

template<typename Iterator>
void client_mode(Iterator first, Iterator last) {
  if (first == last)
    usage("no server, no fun");
  uint32_t init_value = 0;
  vector<pair<string, uint16_t> > remotes;
  string pings_prefix = "--num_pings=";
  auto num_msgs = [&](const string& str) -> maybe<int> {
    if (equal(pings_prefix.begin(), pings_prefix.end(), str.begin())) {
      return spro<int>(str.c_str() + pings_prefix.size());
    }
    return {};
  };
  vector<string> vec;
  for (auto i = first; i != last; ++i) {
    vec.clear();
    split(vec, *i, is_any_of(":"), token_compress_on);
    message_builder{vec.begin(), vec.end()}.apply({
      on(val<string>, spro<int>) >> [&](string& host, int port) {
        if (port <= 1024 || port >= 65536) {
          throw invalid_argument("illegal port: " + to_string(port));
        }
        remotes.emplace_back(move(host), static_cast<uint16_t>(port));
      },
      on(num_msgs) >> [&](int num) {
        if (num > 0) init_value = static_cast<uint32_t>(num);
      }
    });
  }
  if (init_value == 0) {
      cout << "no non-zero, non-negative init value given" << endl;
      exit(1);
  }
  if (remotes.size() < 2) {
      cout << "less than two nodes given" << endl;
      exit(1);
  }
  vector<actor> remote_actors;
  for (auto& r : remotes) {
      remote_actors.push_back(io::remote_actor(r.first.c_str(), r.second));
  }
  // setup phase
  scoped_actor self;
  //cout << "tell server nodes to connect to each other" << endl;
  for (size_t i = 0; i < remotes.size(); ++i) {
      for (size_t j = 0; j < remotes.size(); ++j) {
          if (i != j) {
              auto& r = remotes[j];
              self->send(remote_actors[i], atom("add_pong"),
                               r.first, r.second);
          }
      }
  }
  { // collect {ok} messages
      size_t i = 0;
      size_t end = remote_actors.size() * (remote_actors.size() - 1);
      self->receive_for(i, end) (
          on(atom("ok")) >> [] {
              // nothing to do
          },
          on(atom("error"), arg_match) >> [&](const string& str) {
              cout << "error: " << str << endl;
              for (auto& x : remote_actors) {
                  self->send(x, atom("purge"));
              }
              throw logic_error("");
          },
          others >> [&] {
              cout << "expected {ok|error}, received: "
                   << to_string(self->current_message())
                   << endl;
              throw logic_error("");
          },
          after(chrono::seconds(10)) >> [&] {
              cout << "remote didn't answer within 10sec." << endl;
              for (auto& x : remote_actors) {
                  self->send(x, atom("purge"));
              }
              throw logic_error("");
          }
      );
  }
  // kickoff
  //cout << "setup done" << endl;
  //cout << "kickoff, init value = " << init_value << endl;
  for (auto& r : remote_actors) {
      self->send(r, atom("kickoff"), init_value, self);
  }
  { // collect {done} messages
      size_t i = 0;
      size_t end = remote_actors.size() * (remote_actors.size() - 1);
      self->receive_for(i, end) (
          on(atom("done")) >> [] {
              //cout << "...done..." << endl;
          },
          others >> [&] {
              cout << "unexpected: "
                   << to_string(self->current_message()) << endl;
              throw logic_error("");
          }
      );
  }
}

template<typename Iterator>
void shutdown_mode(Iterator first, Iterator last) {
  vector<pair<string, uint16_t> > remotes;


  vector<string> vec;
  for (auto i = first; i != last; ++i) {
    vec.clear();
    split(vec, *i, is_any_of(":"), token_compress_on);
    message_builder{vec.begin(), vec.end()}.apply({
      on(val<string>, spro<int>) >> [&](string& host, int port) {
        if (port <= 1024 || port >= 65536) {
          throw invalid_argument("illegal port: " + to_string(port));
        }
        remotes.emplace_back(move(host), static_cast<uint16_t>(port));
      }
    });
  }
  scoped_actor self;
  for (auto& r : remotes) {
    try {
      auto x = io::remote_actor(r.first, r.second);
      self->monitor(x);
      self->send(x, atom("shutdown"));
      self->receive (
        on(atom("DOWN"), val<uint32_t>) >> [] {
          // ok, done
        },
        after(chrono::seconds(10)) >> [&] {
          cerr << r.first << ":" << r.second << " didn't shut down "
               << "within 10s"
               << endl;
        }
      );
    }
    catch (std::exception& e) {
      cerr << "couldn't shutdown " << r.first << ":" << r.second
           << "; reason: " << e.what()
           << endl;
    }
  }
}

} // namespace <anonymous>

int main(int argc, char** argv) {
    if (argc < 2) usage();
    auto first = argv + 1;
    auto last = argv + argc;
    message_builder{first, first + 1}.apply({
      on("mode=server") >> [=] {
        server_mode(first + 1, last);
      },
      on("mode=benchmark") >> [=] {
        client_mode(first + 1, last);
      },
      on("mode=shutdown") >> [=] {
        shutdown_mode(first + 1, last);
      },
      (on("-h") || on("--help")) >> [] {
        usage();
      },
      others >> [=] {
        usage("unknown argument: ", *first);
      }
    });
    await_all_actors_done();
    shutdown();
}
