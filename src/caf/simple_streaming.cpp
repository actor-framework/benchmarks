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

constexpr size_t max_samples = 10;

struct source_state {
  size_t count = 0;
  vector<size_t> samples;

  size_t avg() {
    if (samples.empty())
      return 0;
    return accumulate(samples.begin(), samples.end(), size_t{0})
           / samples.size();
  }

  void tick() {
    if (samples.size() == max_samples)
      samples.erase(samples.begin());
    samples.emplace_back(count);
    count = 0;
    cout << avg() << " messages/s" << endl;
  }
};

behavior source(stateful_actor<source_state>* self) {
  self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
  return {
    [=](tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), tick_atom::value);
      self->state.tick();
    },
    [=](start_atom) -> stream<string> {
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

behavior sink(event_based_actor* self, actor src) {
  self->send(self * src, start_atom::value);
  return {
    [=](stream<string>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [](unit_t&, string) {
          // nop
        },
        // cleanup and produce result message
        [](unit_t&) {
          // nop
        }
      );
    }
  };
}

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
    .add(mode, "mode,m", "one of 'sink', 'source', or 'both'")
    .add(port, "port,p", "sets the port of the sink (ignored in 'both' mode)")
    .add(host, "host,o", "sets the host of the sink (only in 'sink' mode)");
    add_message_type<string>("string");
  }

  uint16_t port;
  string host;
  atom_value mode;
};

void caf_main(actor_system& sys, const config& cfg) {
  switch (static_cast<uint64_t>(cfg.mode)) {
    case both_atom::uint_value():
      sys.spawn(sink, sys.spawn(source));
      break;
    case source_atom::uint_value():
      sys.middleman().publish(sys.spawn(source), cfg.port);
      break;
    case sink_atom::uint_value(): {
      auto s = sys.middleman().remote_actor(cfg.host, cfg.port);
      if (!s) {
        cerr << "cannot connect to sink: " << sys.render(s.error()) << endl;
        return;
      }
      sys.spawn(sink, *s);
      break;
    }
  }
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
