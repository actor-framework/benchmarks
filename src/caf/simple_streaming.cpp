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

#ifdef CAF_BEGIN_TYPE_ID_BLOCK

CAF_BEGIN_TYPE_ID_BLOCK(simple_streaming, first_custom_type_id)

  CAF_ADD_TYPE_ID(simple_streaming, (caf::stream<std::string>) )
  CAF_ADD_TYPE_ID(simple_streaming, (std::vector<std::string>) )

  CAF_ADD_ATOM(simple_streaming, start_atom);

CAF_END_TYPE_ID_BLOCK(simple_streaming)

#else

using start_atom = caf::atom_constant<caf::atom("start")>;

static constexpr start_atom start_atom_v = start_atom::value;

#endif

using namespace caf;
using namespace std::chrono_literals;

using std::string;

namespace {

struct tick_state {
  size_t count = 0;

  void tick() {
    std::cout << count << " messages/s\n";
    count = 0;
  }
};

struct source_state : tick_state {
  const char* name = "source";
};

behavior source(stateful_actor<source_state>* self, bool print_rate) {
  if (print_rate)
    self->delayed_send(self, 1s, tick_atom_v);
  return {
    [=](tick_atom) {
      self->delayed_send(self, 1s, tick_atom_v);
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
    },
  };
}

struct stage_state {
  const char* name = "stage";
};

behavior stage(stateful_actor<stage_state>* self) {
  return {
    [=](const stream<string>& in) {
      return self->make_stage(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, downstream<string>& xs, string x) {
          xs.push(std::move(x));
        },
        // cleanup
        [](unit_t&) {
          // nop
        }
      );
    }
  };
}

struct sink_state : tick_state {
  const char* name = "sink";
};

behavior sink(stateful_actor<sink_state>* self, actor src) {
  self->send(self * src, start_atom_v);
  self->delayed_send(self, std::chrono::seconds(1), tick_atom_v);
  return {
    [=](tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), tick_atom_v);
      self->state.tick();
    },
    [=](const stream<string>& in) {
      return self->make_sink(
        // input stream
        in,
        // initialize state
        [](unit_t&) {
          // nop
        },
        // processing step
        [=](unit_t&, string) {
          self->state.count += 1;
        },
        // cleanup
        [](unit_t&) {
          // nop
        }
      );
    }
  };
}

struct config : actor_system_config {
  config() {
#ifdef CAF_BEGIN_TYPE_ID_BLOCK
    io::middleman::init_global_meta_objects();
    init_global_meta_objects<simple_streaming_type_ids>();
#else
    add_message_type<string>("string");
#endif
    opt_group{custom_options_, "global"}
    .add(mode, "mode,m", "one of 'sink', 'source', or 'both'")
    .add(num_stages, "num-stages,n", "number of stages after source / before sink")
    .add(port, "port,p", "sets the port of the sink (ignored in 'both' mode)")
    .add(host, "host,o", "sets the host of the sink (only in 'sink' mode)");
  }

  int num_stages = 0;
  uint16_t port = 0;
  string host = "localhost";
  std::string mode;
};

void caf_main(actor_system& sys, const config& cfg) {
  auto add_stages = [&](actor hdl) {
    for (int i = 0; i < cfg.num_stages; ++i)
      hdl = sys.spawn(stage) * hdl;
    return hdl;
  };
  if (cfg.mode == "both") {
    sys.spawn(sink, add_stages(sys.spawn(source, false)));
  } else if (cfg.mode == "source") {
    sys.middleman().publish(add_stages(sys.spawn(source, true)), cfg.port);
  } else if (cfg.mode == "sink") {
    auto s = sys.middleman().remote_actor(cfg.host, cfg.port);
    if (!s) {
      std::cerr << "cannot connect to source: " << sys.render(s.error())
                << std::endl;
      return;
    }
    sys.spawn(sink, add_stages(*s));
  }
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
