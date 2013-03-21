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

static constexpr size_t Dim = 4000;

template<size_t Rows, size_t Columns = Rows>
class matrix2d {

 public:

    matrix2d(matrix2d&&) = default;
    matrix2d(const matrix2d&) = default;
    matrix2d& operator=(matrix2d&&) = default;
    matrix2d& operator=(const matrix2d&) = default;

    matrix2d() {
        m_data.resize(Rows*Columns);
    }

    matrix2d(vector<float> d) : m_data(std::move(d)) { }

    matrix2d(const std::initializer_list<float>& args) : m_data(args) {
        m_data.resize(Rows*Columns);
    }

    inline float& operator()(size_t row, size_t column) {
        return m_data[row * Rows + column];
        //return m_data[row][column];
    }

    inline const float& operator()(size_t row, size_t column) const {
        return m_data[row * Rows + column];
        //return m_data[row][column];
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

template<size_t R1, size_t C1, size_t R2, size_t C2>
inline bool operator==(const matrix2d<R1,C1>& lhs, const matrix2d<R2,C2>& rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<size_t R1, size_t C1, size_t R2, size_t C2>
inline bool operator!=(const matrix2d<R1,C1>& lhs, const matrix2d<R2,C2>& rhs) {
    return !(lhs == rhs);
}

using matrix_type = matrix2d<Dim>;

matrix_type simple_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    result.zeroize();
    for (size_t i = 0; i < Dim; ++i) {
        for (size_t j = 0; j < Dim; ++j) {
            for (size_t k = 0; k < Dim; ++k) {
                result(i, j) += lhs(i, k) * rhs(k, j);
            }
        }
    }
    return std::move(result);
}

matrix_type actor_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    static constexpr size_t num_workers = Dim;//*Dim;
    matrix_type result;
    actor_ptr master = self;
    result.zeroize();
    for (size_t i = 0; i < Dim; ++i) {
        spawn([&,i,master] {
            for (size_t j = 0; j < Dim; ++j) {
                //spawn([&,i,j,master] {
                    for (size_t k = 0; k < Dim; ++k) {
                        result(i, j) += lhs(i, k) * rhs(k, j);
                    }
                //    send(master, atom("WorkerDone"));
                //});
            }
            send(master, atom("WorkerDone"));
        });
    }
    // await done messages from workers
    size_t i = 0;
    receive_for (i, num_workers) (on(atom("WorkerDone")) >> [] { });
    return std::move(result);
}

matrix_type async_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    matrix_type result;
    vector<future<void>> futures;
    futures.reserve(Dim);//*Dim);
    result.zeroize();
    for (size_t i = 0; i < Dim; ++i) {
      futures.push_back(std::async([&,i] {
        for (size_t j = 0; j < Dim; ++j) {
            //futures.push_back(std::async([&,i,j] {
                for (size_t k = 0; k < Dim; ++k) {
                    result(i, j) += lhs(i, k) * rhs(k, j);
                }
            //}));
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
    actor_ptr operator()(const std::string& source, const std::string& fun_name, Us&&... args) {
        opencl::program p{source};
        auto cd = opencl::get_command_dispatcher();
        return cd->spawn<R, Ts...>(p, fun_name, std::forward<Us>(args)...);
    }
};

template<typename Signature, typename... Ts>
actor_ptr spawn_cl(const std::string& source, const std::string& fun_name, Ts&&... args) {
    cl_spawn_helper<Signature> f;
    return f(source, fun_name, std::forward<Ts>(args)...);
}

#ifdef CPPA_OPENCL
matrix_type opencl_multiply(const matrix_type& lhs, const matrix_type& rhs) {
    static constexpr const char* source = R"__(
        __kernel void mmultiply(__global float* matrix1,
                                __global float* matrix2,
                                __global float* output) {
            int size = 4000;
            int x = get_global_id(0);
            int y = get_global_id(1);
            int idx = 0;
            float result = 0;
            while (idx < size) {
                result += matrix1[idx+y*size] * matrix2[x+idx*size];
                ++idx;
            }
            output[x+y*size] = result;
        }
    )__";
    matrix_type result;
    auto worker = spawn_cl<vector<float> (vector<float>, vector<float>)>(source, "mmultiply", Dim, Dim);
    send(worker, lhs.data(), rhs.data());
    receive(
        on_arg_match >> [&](std::vector<float>& res_vec) {
            result = std::move(res_vec);
        }
    );
    return result;
}
#else
#endif

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
        on("--async", arg_match) >> async_multiply,
        on("--opencl", arg_match) >> opencl_multiply,
        others() >> [] { cerr << "invalid arguments" << endl; }
    );

    return 0;

    /*
    cout << "matrix c:" << endl;
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < columns; ++j) {
            cout << c(i, j) << ' ';
        }
        cout << endl;
    }
    */
    return 0;
}
