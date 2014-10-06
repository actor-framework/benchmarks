#include <cstdio>
#include <cstdlib>

#include <vector>
#include <iostream>

#include "caf/all.hpp"

typedef unsigned char byte;

using namespace std;
using namespace caf;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cout << "usage: ./" << argv[0] << " N" << endl;
    return 0;
  }
  const int    N              = atoi(argv[1]);
  const int    width          = N;
  const int    height         = N;
  const int    max_x          = (width + 7) / 8;
  const int    max_iterations = 250;
  const double limit          = 2.0;
  const double limit_sq       = limit * limit;

  vector<byte> buffer(height * max_x);
  vector<double> cr0(8 * max_x);

  for (int x = 0; x < max_x; ++x) {
    for (int k = 0; k < 8; ++k) {
      const int xk = 8 * x + k;
      cr0[xk] = (2.0 * xk) / width - 1.5;
    }
  }


  for (int y = 0; y < height; ++y) {
    byte* line = &buffer[y * max_x];
    spawn([=] {
      const double ci0 = 2.0 * y / height - 1.0;
      for (int x = 0; x < max_x; ++x) {
        const double* cr0_x = &cr0[8 * x];
        double cr[8];
        double ci[8];
        for (int k = 0; k < 8; ++k) {
            cr[k] = cr0_x[k];
            ci[k] = ci0;
        }
        byte bits = 0xFF;
        for (int i = 0; bits && i < max_iterations; ++i) {
          byte bit_k = 0x80;
          for (int k = 0; k < 8; ++k) {
            if (bits & bit_k) {
              const double cr_k    = cr[k];
              const double ci_k    = ci[k];
              const double cr_k_sq = cr_k * cr_k;
              const double ci_k_sq = ci_k * ci_k;
              cr[k] = cr_k_sq - ci_k_sq + cr0_x[k];
              ci[k] = 2.0 * cr_k * ci_k + ci0;
              if (cr_k_sq + ci_k_sq > limit_sq) {
                bits ^= bit_k;
              }
            }
            bit_k >>= 1;
          }
        }
        line[x] = bits;
      }
    });
  }

  await_all_actors_done();
  shutdown();

  //FILE* out = (argc == 3) ? fopen(argv[2], "wb") : stdout;
  //fprintf(out, "P4\n%u %u\n", width, height);
  //fwrite(&buffer[0], buffer.size(), 1, out);
  //if (out != stdout) {
  //  fclose(out);
  //}
  return 0;
}
