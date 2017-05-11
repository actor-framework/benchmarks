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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <iostream>
#include <vector>
#include <algorithm>
#include <map>

#include "caf/all.hpp"

#include "benchmark_helper.hpp"

using namespace std;
using std::chrono::seconds;
using namespace caf;

class config : public actor_system_config {
public:
  static int n; // = 300
  static int b; // = 50
  static int w; // = 100;

  config() {
    opt_group{custom_options_, "global"}
      .add(n, "nnn,n", "number of workers")
      .add(b, "bbb,b", "block size")
      .add(w, "www,w", "maximum edge weight");
  }
};
int config::n = 300;
int config::b = 50;
int config::w = 100;

template<class T>
using arr2_t = vector<vector<T>>;

template<class T>
arr2_t<T> array_tabulate(size_t y_size, size_t x_size, function<T(size_t, size_t)>&& init_fun) {
  arr2_t<T> result;
  result.reserve(y_size);
  for (size_t y = 0; y < y_size; ++y) {
    vector<T> tmp;
    tmp.reserve(x_size);
    for (size_t x = 0; x < x_size; ++x) {
      tmp.emplace_back(init_fun(y,x));
    }
    result.emplace_back(move(tmp));
  }
  return result;
}

using arr2l = arr2_t<long>;

struct apsp_utils {
  arr2l graph_data;

  void generate_graph() {
    auto n = config::n; 
    auto w = config::w;
    pseudo_random random(n);
    arr2l local_data = array_tabulate<long>(n, n, [](size_t, size_t) { 
      return 0; 
    });
    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        auto r = random.next_int(w) + 1; 
        local_data[i][j] = r;
        local_data[j][i] = r;
      }
    }
    graph_data = move(local_data);
  }

  static arr2l get_block(const arr2l& src_data, int my_block_id) {
    auto n = config::n; 
    auto b = config::b;
    auto local_data = array_tabulate<long>(b, b, [](size_t, size_t) { 
      return 0; 
    });
    auto num_blocks_per_dim = n / b;
    auto global_start_row = (my_block_id / num_blocks_per_dim) * b;
    auto global_start_col = (my_block_id % num_blocks_per_dim) * b;
    for (int i = 0; i < b; ++i) {
      for (int j = 0; j < b; ++j) {
        local_data[i][j] = src_data[i + global_start_row][j + global_start_col];
      }
    }
    return local_data;
  }

  // print content of arr2l
  template<class T>
  static void print(T&& array) {
    ostringstream ss;
    for (auto& a : array) {
      for (auto b : a) {
        ss << b << " ";
      } 
      ss << endl;
    }
    cout << ss.str() << endl;
  }

  // unused function
  //void copy(const arr2l& src_block, const arr2l& dest_array,
            //const tuple<int, int>& offset, int block_size) {
  // ...
  //}
};

using apsp_initial_msg_atom = atom_constant<atom("init")>;

struct apsp_result_msg {
  int k;
  int my_block_id;
  arr2l init_data;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(apsp_result_msg);

struct apsp_neighbor_msg {
  list<actor> neighbors;
};
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(apsp_neighbor_msg);

struct apsp_floyd_warshall_actor_state {
    int num_blocks_in_single_dim;
    int num_neighbors;
    int row_offset;
    int col_offset;
    list<actor> neighbors;
    int k;
    unordered_map<int, arr2l> neighbor_data_per_iteration;
    bool received_neighbors;
    arr2l current_iter_data;
};

behavior apsp_floyd_warshall_actor_fun(
  stateful_actor<apsp_floyd_warshall_actor_state>* self, int my_block_id,
  int block_size, int graph_size, const arr2l& init_graph_data) {
  auto& s = self->state;
  s.num_blocks_in_single_dim = graph_size / block_size;
  s.num_neighbors = 2 * (s.num_blocks_in_single_dim - 1);
  s.row_offset = (my_block_id / s.num_blocks_in_single_dim) * block_size;
  s.col_offset = (my_block_id % s.num_blocks_in_single_dim) * block_size;
  s.k = -1;
  s.received_neighbors = false;
  s.current_iter_data = apsp_utils::get_block(init_graph_data, my_block_id);
  auto store_iteration_data =
    [=](int /*iteration*/, int source_id, arr2l&& data_array) {
      auto& s = self->state;
      s.neighbor_data_per_iteration[source_id] = move(data_array);
      return s.neighbor_data_per_iteration.size()
             == static_cast<size_t>(s.num_neighbors);
    };
  auto element_at = [=](int row, int col, int /*src_iter*/,
                        const arr2l& prev_iter_data) {
    auto& s = self->state;
    auto dest_block_id =
      ((row / block_size) * s.num_blocks_in_single_dim) + (col / block_size);
    auto local_row = row % block_size;
    auto local_col = col % block_size;
    if (dest_block_id == my_block_id) {
      return prev_iter_data[local_row][local_col];
    } else {
      auto& block_data = s.neighbor_data_per_iteration[dest_block_id];
      return block_data[local_row][local_col];
    }
  };
  auto perform_computation = [=] {
    auto& s = self->state;
    auto& prev_iter_data = s.current_iter_data;
    // make modifications on a fresh local data array for this iteration
    s.current_iter_data = array_tabulate<long>(
      block_size, block_size, [](size_t, size_t) { return 0; });
    for (int i = 0; i < block_size; ++i) {
      for (int j = 0; j < block_size; ++j) {
        auto gi = s.row_offset + i;
        auto gj = s.col_offset + j;
        auto new_iter_data = element_at(gi, s.k, s.k - 1, prev_iter_data)
                             + element_at(s.k, gj, s.k - 1, prev_iter_data);
        s.current_iter_data[i][j] = min(prev_iter_data[i][j], new_iter_data);
      }
    }
  };
  auto notify_neighbors = [=] {
    auto& s = self->state;
    // send the current result to all other blocks who might need it
    // note: this is inefficient version where data is sent to neighbors
    // who might not need it for the current value of k
    auto result_message =
      apsp_result_msg{s.k, my_block_id, s.current_iter_data};
    for(auto& loop_neighbor : s.neighbors) {
      self->send(loop_neighbor, result_message); 
    }
  };
  return {
    [=](apsp_result_msg& message) {
      auto& s = self->state;
      if (!s.received_neighbors) {
        cerr << "Block-" << my_block_id << " hasn't received neighbors yet!"
             << endl;
        exit(1);
      }
      auto have_all_data = store_iteration_data(message.k, message.my_block_id,
                                                move(message.init_data));
      if (have_all_data) {
        // received enough data from neighbors, can proceed to do computation
        // for next k
        s.k += 1;
        perform_computation();
        notify_neighbors();
        s.neighbor_data_per_iteration.clear();
        if (s.k == graph_size - 1) {
          // we've completed the computation
          self->quit(); 
        }
      }
    },
    [=](apsp_initial_msg_atom) {
      notify_neighbors(); 
    },
    [=](apsp_neighbor_msg& message) {
      auto& s = self->state;
      auto& msg_neighbors = message.neighbors;
      s.received_neighbors = true;
      for (auto& loop_neighbor : msg_neighbors) {
        s.neighbors.emplace_back(loop_neighbor);
      }
    }
  };
}

void caf_main(actor_system& system, const config& cfg) {
  apsp_utils utils;
  utils.generate_graph();
  auto& graph_data = utils.graph_data;
  auto num_nodes = cfg.n;
  auto block_size = cfg.b;
  int num_blocks_in_single_dim = num_nodes / block_size;
  // create and automatically the actors
  arr2_t<actor> block_actors = array_tabulate<actor>(
    num_blocks_in_single_dim, num_blocks_in_single_dim,
    [&](size_t i, size_t j) -> actor {
      auto my_block_id = (i * num_blocks_in_single_dim) + j;
      auto apsp_actor = system.spawn(apsp_floyd_warshall_actor_fun, my_block_id,
                                     block_size, num_nodes, graph_data);
      return apsp_actor;
  });
  // create the links to the neighbors
  for (int bi = 0; bi < num_blocks_in_single_dim; ++bi) {
    for (int bj = 0; bj < num_blocks_in_single_dim; ++bj) {
      list<actor> neighbors;
      // add neighbors in same column
      for (int r = 0; r < num_blocks_in_single_dim; ++r) {
        if (r != bi) {
          neighbors.emplace_back(block_actors[r][bj]);
        }
      }
      // add neighbors in same row
      for (int c = 0; c < num_blocks_in_single_dim; ++c) {
        if (c != bj) {
          neighbors.emplace_back(block_actors[bi][c]);
        }
      }
      anon_send(block_actors[bi][bj], apsp_neighbor_msg{move(neighbors)});
    }
  }
  // start the computation
  for (int bi = 0; bi < num_blocks_in_single_dim; ++bi) {
    for (int bj = 0; bj < num_blocks_in_single_dim; ++bj) {
      anon_send(block_actors[bi][bj], apsp_initial_msg_atom::value);
    }
  }
}

CAF_MAIN()
