/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

namespace {

using start_atom = atom_constant<atom("start")>;
using tick_atom = atom_constant<atom("tick")>;
using sink_atom = atom_constant<atom("sink")>;
using source_atom = atom_constant<atom("source")>;
using both_atom = atom_constant<atom("both")>;

struct tick_state {
  size_t count = 0;

  void tick() {
    cout << count << " messages/s" << endl;
    count = 0;
  }
};

struct source_state : tick_state {
  const char* name = "source";
};

behavior source(stateful_actor<source_state>* self, bool print_rate) {
  if (print_rate)
    self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
  return {
    [=](tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
      self->state.tick();
    },
    [=](start_atom) {
      return self->make_source(
        // initialize state
        [&](unit_t&) {
          // nop
        },
        // get next element
        [=](unit_t&, downstream<string>& out, size_t num) {
          for (size_t i = 0; i < num; ++i)
            out.push("some data");
          self->state.count += num;
        },
        // check whether we reached the end
        [=](const unit_t&) {
          return false;
        }
      );
    }
  };
}

struct stage_state {
  const char* name = "stage";
};

behavior stage(stateful_actor<stage_state>* self) {
  return {
    [=](stream<string> in) {
      return attach_stream_stage(
        self,
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, downstream<string>& xs, std::vector<string>& x) {
          // xs.push(std::move(x));
          xs.append(std::make_move_iterator(x.begin()),
                    std::make_move_iterator(x.end()));
        },
        // cleanup
        [](unit_t&) {
          // nop
        });
    },
  };
}

struct sink_state : tick_state {
  const char* name = "sink";
};

behavior sink(stateful_actor<sink_state>* self, actor src) {
  self->send(self * src, start_atom::value);
  self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
  return {
    [=](tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
      self->state.tick();
    },
    [=](const stream<string>& in) {
      return attach_stream_sink(
        self,
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, std::vector<string>& xs) {
          self->state.count += xs.size();
        },
        // cleanup
        [](unit_t&) {
          // nop
        });
    },
  };
}

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
    .add(mode, "mode,m", "one of 'sink', 'source', or 'both'")
    .add(num_stages, "num-stages,n", "number of stages after source / before sink")
    .add(port, "port,p", "sets the port of the sink (ignored in 'both' mode)")
    .add(host, "host,o", "sets the host of the sink (only in 'sink' mode)");
    add_message_type<string>("string");
  }

  int num_stages = 0;
  uint16_t port = 0;
  string host = "localhost";
  atom_value mode;
};

void caf_main(actor_system& sys, const config& cfg) {
  auto add_stages = [&](actor hdl) {
    for (int i = 0; i < cfg.num_stages; ++i)
      hdl = sys.spawn(stage) * hdl;
    return hdl;
  };
  switch (static_cast<uint64_t>(cfg.mode)) {
    case both_atom::uint_value():
      sys.spawn(sink, add_stages(sys.spawn(source, false)));
      break;
    case source_atom::uint_value():
      sys.middleman().publish(add_stages(sys.spawn(source, true)), cfg.port);
      break;
    case sink_atom::uint_value(): {
      auto s = sys.middleman().remote_actor(cfg.host, cfg.port);
      if (!s) {
        cerr << "cannot connect to source: " << sys.render(s.error()) << endl;
        return;
      }
      sys.spawn(sink, add_stages(*s));
      break;
    }
  }
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
