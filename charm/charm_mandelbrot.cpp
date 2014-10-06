#include <vector>
#include <iostream>

#include "charm++.h"
#include "charm_mandelbrot.decl.h"

typedef unsigned char byte;

namespace {
size_t m_counter;
std::vector<byte>   m_buffer;
std::vector<double> m_cr0;
}

class main : public CBase_main {
 public:
  main(CkArgMsg* m) {
    if (m->argc != 2) {
      std::cout << std::endl
                << "./charm_mandelbrot PIXELS_PER_DIMENSION"
                << std::endl << std::endl;
      CkExit();
    }
    int N = atoi(m->argv[1]);
    delete m;
    int max_x = (N + 7) / 8;
    int max_iterations = 500;
    double limit = 2.0;
    double limit_sq = limit * limit;
    m_buffer.resize(N * max_x);
    m_cr0.resize(8 * max_x);

    for (int x = 0; x < max_x; ++x) {
      for (int k = 0; k < 8; ++k) {
        const int xk = 8 * x + k;
        m_cr0[xk] = (2.0 * xk) / N - 1.5;
      }
    }
    int num = CkNumPes();
    int pe = 0;
    for (int y = 0; y < N; ++y) {
      CProxy_worker w = CProxy_worker::ckNew(N, max_x, max_iterations, limit_sq, ++pe % num);
      w.calc(y);
    }
    CkPrintf("%s", "main done\n");
  }
};

class worker : public CBase_worker {

  int                        m_dim;
  int                        m_max_x;
  int                        m_max_iters;
  double                     m_limit_sq;

 public:
  worker(int N, int max_x, int max_iters, double limit_sq)
    : m_dim(N),
      m_max_x(max_x),
      m_max_iters(max_iters),
      m_limit_sq(limit_sq) {
    // nop
  }

  void calc(int row) {
    /*
    if (row < m_dim) {
      CProxy_worker w = CProxy_worker::ckNew(m_dim, m_max_x, m_max_iters, m_limit_sq);
      w.calc(row + 1);
    }
    */
    byte* line = &m_buffer[row * m_max_x];
    const double ci0 = 2.0 * row / m_dim - 1.0;
    for (int x = 0; x < m_max_x; ++x) {
      const double* cr0_x = &(m_cr0[8 * x]);
      double cr[8];
      double ci[8];
      for (int k = 0; k < 8; ++k) {
          cr[k] = cr0_x[k];
          ci[k] = ci0;
      }
      byte bits = 0xFF;
      for (int i = 0; bits && i < m_max_iters; ++i) {
        byte bit_k = 0x80;
        for (int k = 0; k < 8; ++k) {
          if (bits & bit_k) {
            const double cr_k    = cr[k];
            const double ci_k    = ci[k];
            const double cr_k_sq = cr_k * cr_k;
            const double ci_k_sq = ci_k * ci_k;
            cr[k] = cr_k_sq - ci_k_sq + cr0_x[k];
            ci[k] = 2.0 * cr_k * ci_k + ci0;
            if (cr_k_sq + ci_k_sq > m_limit_sq) {
              bits ^= bit_k;
            }
          }
          bit_k >>= 1;
        }
      }
      line[x] = bits;
    }
    delete this;
    if (__sync_add_and_fetch(&m_counter, 1) == m_dim) {
      /*
      FILE* out = fopen("chaming mandelbrot.bmp", "wb");
      fprintf(out, "P4\n%u %u\n", m_dim, m_dim);
      fwrite(&m_buffer[0], m_buffer.size(), 1, out);
      if (out != stdout) {
        fclose(out);
      }
      */
      // done
      CkExit();
    }
  }
};

#include "charm_mandelbrot.def.h"
