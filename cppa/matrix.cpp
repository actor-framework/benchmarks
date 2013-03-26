/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <array>
#include <vector>
#include <future>
#include <numeric>
#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

using namespace std;
using namespace cppa;

static constexpr size_t matrix_size = 1000;

template<size_t Size>
class square_matrix {

 public:

    static constexpr size_t num_elements = Size * Size;

    square_matrix(square_matrix&&) = default;
    square_matrix(const square_matrix&) = default;
    square_matrix& operator=(square_matrix&&) = default;
    square_matrix& operator=(const square_matrix&) = default;

    square_matrix() {
        m_data.resize(num_elements);
    }

    square_matrix(vector<float> d) : m_data(std::move(d)) { }

    square_matrix(const std::initializer_list<float>& args) : m_data(args) {
        m_data.resize(num_elements);
    }

    inline float& operator()(size_t row, size_t column) {
        return m_data[row * Size + column];
    }

    inline const float& operator()(size_t row, size_t column) const {
        return m_data[row * Size + column];
    }

    inline void zeroize() {
        std::fill(m_data.begin(), m_data.end(), 0);
    }

    inline void iota_fill() {
        std::iota(m_data.begin(), m_data.end(), 0);
    }

    typedef typename vector<float>::const_iterator const_iterator;

    const_iterator begin() const { return m_data.begin(); }

    const_iterator end() const { return m_data.end(); }

    vector<float>& data() { return m_data; }

    const vector<float>& data() const { return m_data; }

 private:

    vector<float> m_data;

};

template<size_t Size>
inline bool operator==(const square_matrix<Size>& lhs, const square_matrix<Size>& rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<size_t Size>
inline bool operator!=(const square_matrix<Size>& lhs, const square_matrix<Size>& rhs) {
    return !(lhs == rhs);
}

using matrix_type = square_matrix<matrix_size>;

float dot_product(const matrix_type& lhs, const matrix_type& rhs, size_t row, size_t column) {
    float result = 0.0f;
    for (size_t k = 0; k < matrix_size; ++k) {
        result += lhs(row, k) * rhs(k, column);
    }
    return result;
}

matrix_type simple_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    for (size_t row = 0; row < matrix_size; ++row) {
        for (size_t column = 0; column < matrix_size; ++column) {
            result(row, column) = dot_product(lhs, rhs, row, column);
        }
    }
    return std::move(result);
}

matrix_type actor_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    for (size_t row = 0; row < matrix_size; ++row) {
        for (size_t column = 0; column < matrix_size; ++column) {
            spawn<monitored>([&,row,column] {
                result(row, column) = dot_product(lhs, rhs, row, column);
            });
        }
    }
    await_all_others_done();
    return std::move(result);
}

matrix_type actor_multiply2(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    for (size_t row = 0; row < matrix_size; ++row) {
        spawn<monitored>([&,row] {
            for (size_t column = 0; column < matrix_size; ++column) {
                result(row, column) = dot_product(lhs, rhs, row, column);
            }
        });
    }
    await_all_others_done();
    return std::move(result);
}

matrix_type async_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    vector<future<void>> futures;
    futures.reserve(matrix_size * matrix_size);
    for (size_t row = 0; row < matrix_size; ++row) {
        for (size_t column = 0; column < matrix_size; ++column) {
            futures.push_back(std::async(std::launch::deferred, [&,row,column] {
                result(row, column) = dot_product(lhs, rhs, row, column);
            }));
        }
    }
    for (auto& f : futures) f.wait();
    return std::move(result);
}

matrix_type async_multiply2(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    vector<future<void>> futures;
    futures.reserve(matrix_size);
    for (size_t row = 0; row < matrix_size; ++row) {
        futures.push_back(std::async(std::launch::deferred, [&,row] {
            for (size_t column = 0; column < matrix_size; ++column) {
                result(row, column) = dot_product(lhs, rhs, row, column);
            }
        }));
    }
    for (auto& f : futures) f.wait();
    return std::move(result);
}

template<typename Signature>
struct cl_spawn_helper;

template<typename R, typename... Ts>
struct cl_spawn_helper<R (Ts...)> {
    template<typename... Us>
    actor_ptr operator()(const char* source, const char* fun_name, Us&&... args) {
        auto p = opencl::program::create(source);
        auto cd = opencl::get_command_dispatcher();
        return cd->spawn<R, Ts...>(p, fun_name, std::forward<Us>(args)...);
    }
};

template<typename Signature, typename... Ts>
actor_ptr spawn_cl(const char* source, const char* fun_name, Ts&&... args) {
    cl_spawn_helper<Signature> f;
    return f(source, fun_name, std::forward<Ts>(args)...);
}

matrix_type opencl_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    static constexpr const char* source = R"__(
        __kernel void multiply(__global float* lhs,
                               __global float* rhs,
                               __global float* result) {
            size_t size = get_global_size(0); // rows == columns
            size_t row = get_global_id(0);
            size_t column = get_global_id(1);
            float dot_product = 0;
            for (size_t k = 0; k < size; ++k) {
                dot_product += lhs[k + column * size] * rhs[row + k * size];
            }
            result[row + column * size] = dot_product;
        }
    )__";
    matrix_type result;
    auto worker = spawn_cl<vector<float> (const vector<float>&, const vector<float>&)>(source, "multiply", matrix_size, matrix_size);
    /*
    auto worker = spawn_cl<vector<float> (vector<float>, vector<float>)>(source, "multiply", matrix_size, matrix_size,
        [](const std::function<void (vector<float>, vector<float>)>& enqueue) -> partial_function {
            return (
                on_arg_match >> [fptr](const matrix_type& lhs, const matrix_type& rhs) {
                    enqueue(fptr(lhs.data(), rhs.data()));
                }
            );
        },
        [](const response_handle& handle, const vector<float>& result) {
            reply_to(handle, result);
        }
   );
   */
   send(worker, lhs.data(), rhs.data());
    receive(
        on_arg_match >> [&](std::vector<float>& res_vec) {
            result = std::move(res_vec);
        }
    );
    return result;
}

int main(int argc, char** argv) {

    announce<vector<int>>();
    announce<vector<float>>();
    announce<matrix_type>();

    if (argc != 2) {
        cerr << "usage: " << argv[0] << " --(simple|actor|async|opencl)" << endl;
        return -1;
    }

    matrix_type a;
    a.iota_fill();
    matrix_type b;
    b.iota_fill();

    match(make_any_tuple(argv[1], std::move(a), std::move(b))) (
        on("--simple", arg_match) >> simple_multiply,
        on("--actor", arg_match) >> actor_multiply,
        on("--actor2", arg_match) >> actor_multiply2,
        on("--async", arg_match) >> async_multiply,
        on("--async2", arg_match) >> async_multiply2,
        on("--opencl", arg_match) >> opencl_multiply,
        others() >> [] { cerr << "invalid arguments" << endl; }
    );

    return 0;
}
