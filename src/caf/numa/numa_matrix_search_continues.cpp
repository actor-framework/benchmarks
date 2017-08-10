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

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using search_atom = atom_constant<atom("search")>;
using result_atom = atom_constant<atom("result")>;
using quit_atom = atom_constant<atom("quit")>;

enum class search_pattern {
 horizontal, vertical
};

using search_t = string;
using matrix_t = vector<search_t>;

class generate_next_line {
public:
  generate_next_line(int id, size_t x_size, size_t y_size)
      : rengine_(id + x_size + y_size)
      , uniform_('a', 'z')
      , x_size_(x_size)
      , y_size_(y_size)
      , generated_lines_(0) {
  }

  // generate a random strings
  search_t operator()() {
    search_t result;
    result.reserve(x_size_);
    for (size_t i = 0; i < x_size_; ++i) {
      result.push_back(uniform_(rengine_));
    }
    ++generated_lines_;
    return result;
  }

  bool has_next() {
    return generated_lines_ < y_size_;
  }
private:
  default_random_engine rengine_;
  uniform_int_distribution<char> uniform_;
  size_t x_size_;
  size_t y_size_;
  size_t generated_lines_;
};

behavior matrix_searcher(stateful_actor<matrix_t>* self, actor controller,
                         int seed, size_t id, size_t matrix_size) {
  generate_next_line get_line(seed * id, matrix_size, matrix_size);
  search_t line;
  while(get_line.has_next()) {
    line = get_line(); 
    self->state.emplace_back(move(line));
  }
  return {
    [=](quit_atom) {
      self->quit();
    },
    [=](search_atom, search_pattern pattern, const search_t& search_obj) {
      matrix_t& matrix = self->state;
      uint64_t result = 0;
      if (pattern == search_pattern::horizontal) {
        for (auto& line : matrix) {
          for (auto pos = line.find(search_obj); pos != string::npos;
               pos = line.find(search_obj, pos + 1)) {
            ++result;
          }
        } 
      } else if (pattern == search_pattern::vertical) { 
        for (size_t y = 0; y < matrix.size(); ++y) {
          for (auto x_pos = matrix[y].find(search_obj[0]); x_pos != string::npos;
               x_pos = matrix[y].find(search_obj[0], x_pos + 1)) {
            if (x_pos != string::npos) {
              size_t i = 1;
              for (size_t y1 = y + 1; y1 < matrix.size() && i < search_obj.size(); ++y1) {
                if (matrix[y1][x_pos] != search_obj[i]) {
                  break;
                } else if (i == search_obj.size() - 1) {
                  ++result;
                }
                ++i;
              }
            }
          }
        } 
      }
      self->send(controller, result_atom::value, id, result);
    }
  };
}

struct matrix_searcher_props {
  actor matrix_searcher;
  size_t num_of_searches;
};

struct controller_data {
  vector<matrix_searcher_props> matrix_searchers;
  vector<search_t> search_list;
  size_t num_finished_matrix_searcher;
  uint64_t num_findings;
  default_random_engine rengine;
  uniform_int_distribution<char> uniform;
};

behavior controller(stateful_actor<controller_data>* self, int controller_id,
                    size_t num_matrix_searchers, search_pattern pattern,
                    size_t matrix_size, int search_size, int num_searches) {
  auto& s = self->state;
  // create matrix searcher (worker)
  for (size_t id = 0; id < num_matrix_searchers; ++id) {
    auto ms =
      actor_cast<actor>(self->spawn(matrix_searcher, actor_cast<actor>(self),
                                    controller_id, id, matrix_size));
    s.matrix_searchers.emplace_back(matrix_searcher_props{move(ms), 0});
  }
  // generate all search requests
  s. rengine = default_random_engine(num_matrix_searchers + matrix_size);
  s.uniform = uniform_int_distribution<char>('a', 'z');
  auto get_random_search_obj = [=](){
    auto& s = self->state;
    search_t result;
    result.reserve(search_size);
    for (int j = 0; j < search_size; ++j) {
      result.push_back(s.uniform(s.rengine));
    }
    return result;
  };
  for(int i = 0; i < num_searches; ++i) {
    s.search_list.emplace_back(get_random_search_obj()); 
  }
  // how to send a search request
  auto send_search_request = [=](size_t ms_id){
    auto& s = self->state;
    auto& ms = s.matrix_searchers[ms_id];
    if (ms.num_of_searches < s.search_list.size()) {
      self->send(ms.matrix_searcher, search_atom::value, pattern,
                 s.search_list[ms.num_of_searches]);
      ++ms.num_of_searches;
      return true;
    } else {
      return false; 
    }
  };
  // send a search request to matrix searcher
  for(size_t i = 0; i < s.matrix_searchers.size(); ++i) {
    send_search_request(i);
  }
  // receive and resend search requests until all search requests are processed 
  return {
    [=](result_atom, size_t ms_id, uint64_t num_findings) {
      auto& s = self->state;
      s.num_findings += num_findings;
      if(!send_search_request(ms_id)) {
        ++s.num_finished_matrix_searcher;
      }
      if (s.num_finished_matrix_searcher == s.matrix_searchers.size()) {
        for (size_t i = 0; i < s.matrix_searchers.size(); ++i) {
          self->send(s.matrix_searchers[i].matrix_searcher, quit_atom::value);
        }
        aout(self) << "findings: " << s.num_findings << endl;
        self->quit();
      }
    }
  };
}

class config : public actor_system_config {
public:
  int num_matrix_searchers = 15;
  int num_searches = 30;
  int num_controllers = 15;
  int search_size = 6;
  string search_pattern = "v"; //v=vertical, h=horizontal
  size_t matrix_size = 3500;

  config() {
    opt_group{custom_options_, "global"}
      .add(num_matrix_searchers, "worker", "number of workers")
      .add(num_searches, "searches", "number of searches")
      .add(num_controllers, "searches", "number of searches")
      .add(search_size, "search-size", "size of each search")
      .add(search_pattern, "pattern",
           "search pattern (h=horizontal, v=vertical) ")
      .add(matrix_size, "msize", "size of the matrix");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  search_pattern pattern;
  if (cfg.search_pattern == "h" ) {
    pattern = search_pattern::horizontal;
  } else if (cfg.search_pattern == "v" ) {
    pattern = search_pattern::vertical;
  } else {
    cerr << "Unknown search pattern: " << cfg.search_pattern << endl;
    return;
  }
  for(int id = 0; id < cfg.num_controllers; ++id) {
    system.spawn(controller, id, cfg.num_matrix_searchers, pattern, cfg.matrix_size,
                 cfg.search_size, cfg.num_searches);
  }
  system.await_all_actors_done();
}

CAF_MAIN()
