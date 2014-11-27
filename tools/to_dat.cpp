#include <map>
#include <cmath>
#include <array>
#include <regex>
#include <vector>
#include <string>
#include <numeric>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

#include <boost/math/distributions/students_t.hpp>

using namespace std;

struct variance_plus {
  double mean;
  variance_plus(double mean) : mean(mean) {
    // nop
  }
  double operator()(double res, double a) {
    auto tmp = mean - a;
    return res + (tmp * tmp);
  }
};

struct statistics {
  double mean;
  double variance;
  double std_dev;
  double conf_interval_95;
  statistics(const vector<double>& data) {
    if (data.empty()) {
      return;
    }
    if (data.size() == 1) {
      mean = data.front();
      variance = 0;
      std_dev = 0;
      conf_interval_95 = 0;
      return;
    }
    using namespace boost::math;
    mean = accumulate(data.begin(), data.end(), 0., plus<double>{})
           / data.size();
    variance = accumulate(data.begin(), data.end(), 0., variance_plus{mean})
               / data.size();
    std_dev = sqrt(variance);
    // calculate confidence interval
    students_t dist{static_cast<double>(data.size() - 1)};
    // t-statistic for 95% confidence interval
    double tstat = quantile(complement(dist, 0.025 / 2));
    // width of confidence interval
    conf_interval_95 = tstat * std_dev / sqrt(static_cast<double>(data.size()));
  }
};

int main(int argc, char** argv) {
  map<string, string> nice_names{{"caf",     "CAF"},
                                 {"scala",   "Scala"},
                                 {"salsa",   "SalsaLite"},
                                 {"theron",  "Theron"},
                                 {"go",      "GoLang"},
                                 {"charm",   "Charm"},
                                 {"foundry", "ActorFoundry"},
                                 {"erlang",  "Erlang"}};
  vector<string> files(argv + 1, argv + argc);
  regex fname_rx("([0-9]+)_cores_runtime_"
                 "(caf|scala|salsa|theron|erlang|go|foundry|charm)_"
                 "([a-zA-Z_]+)\\.txt");
  smatch fname_match;
  // $benchmark => {$lang => {$num_cores => [$values]}}
  map<string, map<string, map<size_t, vector<double>>>> samples;
  for (auto& fname : files) {
    if (regex_match(fname, fname_match, fname_rx) && fname_match.size() == 4) {
      auto num_cores = static_cast<size_t>(stoi(fname_match.str(1)));
      auto lang = fname_match.str(2);
      auto bench = fname_match.str(3);
      vector<double>& values = samples[bench][lang][num_cores];
      ifstream f{fname};
      double value;
      while (f >> value) {
        // convert ms to s
        values.push_back(value / 1000.0);
      }
    } else {
      cerr << "file name \"" << fname << "\" does not match regex" << endl;
    }
  }
  if (samples.empty()) {
    cout << "... nothing to do ..." << endl;
    return 0;
  }
  // calculate filed width from maximum field name + "_yerr"
  string yerr_suffix = "_yerr";
  size_t field_width = 0;
  for (auto& nn : nice_names) {
    field_width = max(field_width, nn.second.size() + yerr_suffix.size());
  }
  for (auto& kvp : samples) {
    auto& benchmark = kvp.first;
    ostringstream tmp;
    tmp << left;
    tmp << setw(field_width) << "cores";
    map<size_t, map<string, pair<double, double>>> output_table;
    for (auto& kvp2 : kvp.second) {
      auto& lang = kvp2.first;
      tmp << " " << setw(field_width) << nice_names[lang] << " "
          << setw(field_width) << (nice_names[lang] + yerr_suffix);
      for (auto& kvp3 : kvp2.second) {
        auto num_cores = kvp3.first;
        statistics stats{kvp3.second};
        auto yerr = kvp3.second.size() < 9 ? 0. : stats.conf_interval_95;
        output_table[num_cores][lang] = make_pair(stats.mean, yerr);
      }
    }
    auto file_header = tmp.str();
    ofstream oss{benchmark + ".dat"};
    oss << left;
    oss << file_header << endl;
    for (auto& output_kvp : output_table) {
      oss << setw(field_width) << output_kvp.first; // number of cores
      for (auto& inner_kvp : output_kvp.second) {
        // print mean and 95% confidence interval
        oss << " " << setw(field_width) << inner_kvp.second.first << " "
            << setw(field_width) << inner_kvp.second.second;
      }
      oss << endl;
    }
  }
}
