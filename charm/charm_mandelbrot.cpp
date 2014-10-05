#include <vector>
#include <iostream>

#include "charm++.h"
#include "charm_mandelbrot.decl.h"

typedef unsigned char byte;

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
    int max_iterations = 50;
    double limit = 2.0;
    double limit_sq = limit * limit;
    std::vector<byte> buffer(N * max_x);
    std::vector<double> cr0(8 * max_x);

    for (int x = 0; x < max_x; ++x) {
      for (int k = 0; k < 8; ++k) {
        const int xk = 8 * x + k;
        cr0[xk] = (2.0 * xk) / N - 1.5;
      }
    }
    std::cout << "Starting with counter creation ..." << std::endl;
    CProxy_counter cnt = CProxy_counter::ckNew(N, buffer);
    std::cout << "Starting with worker creation ... " << std::endl;
    for (int y = 0; y < N; ++y) {
      CkPrintf("%s%i%s", "Creating worker ", y, ".\n");
      CProxy_worker w = CProxy_worker::ckNew(cnt, N, max_x, max_iterations, limit_sq, buffer, cr0);
      w.calc(y);
    }
  }
};

class worker : public CBase_worker {

  CProxy_counter            m_cnt;
  int                        m_dim;
  int                        m_max_x;
  int                        m_max_iters;
  double                     m_limit_sq;
  //byte*                      m_line;
  std::vector<byte>&         m_buffer;
  const std::vector<double>& m_cr0;

 public:
  worker(CProxy_counter cnt, int N, int max_x, int max_iters, 
         double limit_sq, std::vector<byte>& buffer, const std::vector<double>& cr0)
    : m_cnt(cnt), 
      m_dim(N),
      m_max_x(max_x),
      m_max_iters(max_iters),
      m_limit_sq(limit_sq),
      //m_line(line),
      m_buffer(buffer),
      m_cr0(cr0) { }

  void calc(int row) {
    CkPrintf("%s%i%s", "Starting to calculate row ", row, ".\n");
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
    CkPrintf("%s%i%s", "Done with row ", row, ".\n");
    m_cnt.msg();
    delete this;
  }
};

class counter : public CBase_counter {
 public:
  counter(uint64_t max, const std::vector<byte>& buffer)
    : m_max(max), m_value(0), m_buffer(buffer) { }

  void msg() {
    if (++m_value == m_max) {
      CkPrintf("%s", "Mandelbrot is done\n");
      FILE* out = stdout;
      fprintf(out, "P4\n%u %u\n", m_max, m_max);
      fwrite(&m_buffer[0], m_buffer.size(), 1, out);
      if (out != stdout) {
        fclose(out);
      }
      CkExit();
    }
    CkPrintf("%s%i%s", "Received ", m_value, " done messages\n");
  }

 private:
  int m_max;
  int m_value;
  const std::vector<byte>& m_buffer;
};

#include "charm_mandelbrot.def.h"
