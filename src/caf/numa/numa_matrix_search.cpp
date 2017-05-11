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

//-- generic in matrix search engine --
using init_atom = atom_constant<atom("init")>;
using search_atom = atom_constant<atom("search")>;

template <class T>
using matrix_t = vector<T>;
template <class T>
using search_fun_t = function<uint64_t(const matrix_t<T>&, const T&)>;

template <class T, class U>
behavior worker(stateful_actor<matrix_t<T>>* self, int id) {
  return {
    [=](init_atom, size_t x_size, size_t y_size) {
      self->state.clear();
      U get_line(id, x_size, y_size);
      T line;
      while(get_line.has_next()) {
        line = get_line(); 
        self->state.emplace_back(move(line));
      }
    },
    [=](search_atom, const T& obj, const search_fun_t<T>& search_fun) {
      return search_fun(self->state, obj);
    },
  };
}

struct controller_data {
  vector<strong_actor_ptr> workers;
  uint64_t result_count;
  size_t worker_finished;
};

template <class T, class U>
behavior controller(stateful_actor<controller_data>* self, size_t num_workers) {
  for (size_t id = 0; id < num_workers; ++id) {
    self->state.workers.emplace_back(
      actor_cast<strong_actor_ptr>(self->spawn(worker<T, U>, id)));
  }
  return {
    [=](init_atom, size_t x_size, size_t y_size) {
      for (auto& w : self->state.workers) {
        self->send(actor_cast<actor>(w), init_atom::value, x_size, y_size);
      }
    },
    [=](search_atom, const T& obj, const search_fun_t<T>& search_fun) {
      self->state.result_count = 0;
      self->state.worker_finished = 0;
      auto rp = self->template make_response_promise<uint64_t>();
      for (auto& e : self->state.workers) {
        self
          ->request(actor_cast<actor>(e), infinite, search_atom::value,
                    obj, search_fun)
          .then([=](uint64_t result_count) mutable {
            self->state.result_count += result_count;
            ++self->state.worker_finished;
            if (self->state.worker_finished >= self->state.workers.size()) {
              rp.deliver(self->state.result_count);
            }
          });
      }
      return rp;
    }};
}

//-- string search specification --

class config : public actor_system_config {
public:
  int num_workers = 300;
  int num_searches = 100;
  int search_size = 5;
  string search_pattern = "v"; //v=vertical, h=horizontal
  size_t y_size = 5000;
  size_t x_size = 250;


  config() {
    opt_group{custom_options_, "global"}
      .add(num_workers, "worker", "number of workers")
      .add(num_searches, "searches", "number of searches")
      .add(search_size, "search-size", "size of each search")
      .add(search_pattern, "pattern",
           "search pattern (h=horizontal, v=vertical) ")
      .add(y_size, "ysize", "y size of the matrix")
      .add(x_size, "xsize", "x size of the matrix");
  }
};

using line_t = string;
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(search_fun_t<line_t>);

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
  line_t operator()() {
    line_t result;
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

enum class search_pattern {horizontal = 1, vertical = 2};

void caf_main(actor_system& system, const config& cfg) {
  auto search_fun = [](const matrix_t<line_t> matrix,
                                       const line_t& str, search_pattern d) {
    uint64_t result = 0;
    if (str.size() <= 0)
      return result;  
    switch(d) {
      case search_pattern::horizontal: {
        for (auto& line : matrix) {
          for (auto pos = line.find(str); pos != string::npos;
               pos = line.find(str, pos + 1)) {
            ++result;
          }
        }
        break;
      }
      case search_pattern::vertical: {
        for (size_t y = 0; y < matrix.size(); ++y) {
          for (auto x_pos = matrix[y].find(str[0]); x_pos != string::npos;
               x_pos = matrix[y].find(str[0], x_pos + 1)) {
            if (x_pos != string::npos) {
              size_t i = 1;
              for (size_t y1 = y + 1; y1 < matrix.size() && i < str.size(); ++y1) {
                if (matrix[y1][x_pos] != str[i]) {
                  break;
                } else if (i == str.size() - 1) {
                  ++result;
                }
                ++i;
              }
            }
          }
        } 
        break;                           
      }
    }
    return result;
  };
  search_pattern pattern;
  if (cfg.search_pattern == "h" ) {
    pattern = search_pattern::horizontal;
  } else if (cfg.search_pattern == "v" ) {
    pattern = search_pattern::vertical;
  } else {
    cerr << "Unknown search pattern: " << cfg.search_pattern << endl;
    return;
  }
  auto c =
    system.spawn<detached>(controller<line_t, generate_next_line>, cfg.num_workers);
    //system.spawn<detached>(controller<line_t, generate_next_line>, cfg.num_workers);
  scoped_actor self{system};
  self->send(c, init_atom::value, cfg.x_size, cfg.y_size);
  line_t search_obj;
  default_random_engine rengine(cfg.num_workers + cfg.x_size + cfg.y_size);
  uniform_int_distribution<char> uniform('a', 'z');
  for (int i = 0; i < cfg.num_searches; ++i) {
    search_obj.clear();
    for (int j = 0; j < cfg.search_size; ++j) {
      search_obj.push_back(uniform(rengine));
    }
    self->send(c, search_atom::value, search_obj,
               static_cast<search_fun_t<line_t>>(
                 [=](const matrix_t<line_t>& matrix, const line_t& obj) {
                   return search_fun(matrix, obj, pattern);
                 }));
    self->receive([&](uint64_t num_results) {
      cout << "findings for " << search_obj << ": " << num_results << std::endl;
    });
  }
}

CAF_MAIN()
