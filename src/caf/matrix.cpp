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

#include <array>
#include <vector>
#include <future>
#include <numeric>
#include <iostream>

#include "caf/all.hpp"

#ifdef ENABLE_OPENCL
#include "caf/opencl/all.hpp"
#endif

using namespace std;
using namespace caf;

static constexpr size_t matrix_size = 1000;

template <size_t Size>
class square_matrix {
public:
  static constexpr size_t num_elements = Size * Size;

  using value_type = float;

  square_matrix(square_matrix&&) = default;
  square_matrix(const square_matrix&) = default;
  square_matrix& operator=(square_matrix&&) = default;
  square_matrix& operator=(const square_matrix&) = default;

  square_matrix() {
    data_.resize(num_elements);
  }

  square_matrix(vector<float> d) : data_(std::move(d)) {
    // nop
  }

  square_matrix(const std::initializer_list<float>& args) : data_(args) {
    data_.resize(num_elements);
  }

  inline float& operator()(size_t row, size_t column) {
    return data_[row * Size + column];
  }

  inline const float& operator()(size_t row, size_t column) const {
    return data_[row * Size + column];
  }

  inline void zeroize() {
    std::fill(data_.begin(), data_.end(), 0);
  }

  inline void iota_fill() {
    std::iota(data_.begin(), data_.end(), 0);
  }

  typedef typename vector<float>::const_iterator const_iterator;

  const_iterator begin() const {
    return data_.begin();
  }

  const_iterator end() const {
    return data_.end();
  }

  vector<float>& data() {
    return data_;
  }

  const vector<float>& data() const {
    return data_;
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 square_matrix& x) {
    return f(meta::type_name("square_matrix"), x.data_);
  }

private:
  vector<float> data_;
};

template<size_t Size>
bool operator==(const square_matrix<Size>& x, const square_matrix<Size>& y) {
    return std::equal(x.begin(), x.end(), y.begin());
}

template<size_t Size>
bool operator!=(const square_matrix<Size>& x, const square_matrix<Size>& y) {
    return !(x == y);
}

using matrix_type = square_matrix<matrix_size>;

float dot_product(const matrix_type& x,
                  const matrix_type& y,
                  size_t row, size_t column) {
  float result = 0.0f;
  for (size_t k = 0; k < matrix_size; ++k)
    result += x(row, k) * y(k, column);
  return result;
}

matrix_type simple_multiply(const matrix_type& x, const matrix_type& y) {
  matrix_type result;
  for (size_t row = 0; row < matrix_size; ++row)
    for (size_t column = 0; column < matrix_size; ++column)
      result(row, column) = dot_product(x, y, row, column);
  return result;
}

matrix_type actor_multiply(const matrix_type& x, const matrix_type& y) {
  actor_system_config cfg;
  actor_system system{cfg};
  matrix_type result;
  for (size_t row = 0; row < matrix_size; ++row)
    for (size_t column = 0; column < matrix_size; ++column)
      system.spawn([&, row, column] {
        result(row, column) = dot_product(x, y, row, column);
      });
  return result;
}

matrix_type actor_multiply2(const matrix_type& x, const matrix_type& y) {
  actor_system_config cfg;
  actor_system system{cfg};
  matrix_type result;
  for (size_t row = 0; row < matrix_size; ++row)
    system.spawn([&, row] {
      for (size_t column = 0; column < matrix_size; ++column) {
        result(row, column) = dot_product(x, y, row, column);
      }
    });
  return result;
}

#if defined(CAF_GCC) && defined(CAF_MACOS)
matrix_type async_multiply(const matrix_type&, const matrix_type&) {
  throw std::logic_error("Not available on this platform");
}

matrix_type async_multiply2(const matrix_type&, const matrix_type&) {
  throw std::logic_error("Not available on this platform");
}
#else // defined(CAF_GCC) && defined(CAF_MACOS)
matrix_type async_multiply(const matrix_type& x, const matrix_type& y) {
  matrix_type result;
  vector<future<void>> futures;
  futures.reserve(matrix_size * matrix_size);
  for (size_t row = 0; row < matrix_size; ++row) {
    for (size_t column = 0; column < matrix_size; ++column) {
      futures.push_back(std::async(std::launch::async, [&, row, column] {
        result(row, column) = dot_product(x, y, row, column);
      }));
    }
  }
  for (auto& f : futures)
    f.wait();
  return result;
}

matrix_type async_multiply2(const matrix_type& x, const matrix_type& y) {
  matrix_type result;
  vector<future<void>> futures;
  futures.reserve(matrix_size);
  for (size_t row = 0; row < matrix_size; ++row) {
    futures.push_back(std::async(std::launch::async, [&,row] {
      for (size_t column = 0; column < matrix_size; ++column) {
        result(row, column) = dot_product(x, y, row, column);
      }
    }));
  }
  for (auto& f : futures)
    f.wait();
  return result;
}
#endif // defined(CAF_GCC) && defined(CAF_MACOS)

#ifdef ENABLE_OPENCL
matrix_type opencl_multiply(const matrix_type& x, const matrix_type& y) {
    static constexpr const char* source = R"__(
        __kernel void multiply(__global float* x,
                               __global float* y,
                               __global float* result) {
            size_t size = get_global_size(0); // rows == columns
            size_t row = get_global_id(0);
            size_t column = get_global_id(1);
            float dot_product = 0;
            for (size_t k = 0; k < size; ++k) {
                dot_product += x[k + column * size] * y[row + k * size];
            }
            result[row + column * size] = dot_product;
        }
    )__";
    matrix_type result;
    auto worker = spawn_cl<vector<float>(const vector<float>&,
                                         const vector<float>&)>(source,
                                                                "multiply",
                                                                matrix_size,
                                                                matrix_size);
   send(worker, x.data(), y.data());
    receive(
        on_arg_match >> [&](std::vector<float>& res_vec) {
            result = std::move(res_vec);
        }
    );
    return result;
}
#else
matrix_type opencl_multiply(const matrix_type&, const matrix_type&) {
  fprintf(stderr, "Compiled w/o OpenCL support");
  abort();
}
#endif

int main(int argc, char** argv) {
  if (argc != 2) {
    cerr << "usage: " << argv[0] << " --(simple|actor|async|opencl)" << endl;
    return -1;
  }
  using fun = matrix_type (*)(const matrix_type&, const matrix_type&);
  std::map<string, fun> funs{
    {"--simple", simple_multiply},
    {"--actor", actor_multiply},
    {"--actor2", actor_multiply2},
    {"--async", async_multiply},
    {"--async2", async_multiply2},
    {"--opencl", opencl_multiply}
  };
  auto x = funs.find(argv[1]);
  if (x == funs.end()) {
    cerr << "invalid command line option" << endl;
    return -1;
  }
  matrix_type a;
  a.iota_fill();
  matrix_type b;
  b.iota_fill();
  (x->second)(a, b);
}
