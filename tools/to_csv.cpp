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

// for CAF_PUSH_WARNINGS
#include "caf/config.hpp"
#include "caf/string_algorithms.hpp"

CAF_PUSH_WARNINGS
#include <boost/math/distributions/students_t.hpp>
CAF_POP_WARNINGS

using namespace std;

struct variance_plus {
  double mean;
  variance_plus(double mean_value) : mean(mean_value) {
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
           / static_cast<double>(data.size());
    variance = accumulate(data.begin(), data.end(), 0., variance_plus{mean})
               / static_cast<double>(data.size());
    std_dev = sqrt(variance);
    // calculate confidence interval
    students_t dist{static_cast<double>(data.size() - 1)};
    // t-statistic for 95% confidence interval
    double tstat = quantile(complement(dist, 0.025 / 2));
    // width of confidence interval
    conf_interval_95 = tstat * std_dev / sqrt(static_cast<double>(data.size()));
  }
};

using file_name = std::string;

enum benchmark_file_type {
  runtime_values,
  memory_values,
  invalid_file
};

struct benchmark_file {
  string path;
  size_t num_units;
  benchmark_file_type type;
  string framework;
  string benchmark_name;
};

bool bench_name_cmp(const benchmark_file& lhs, const benchmark_file& rhs) {
  return lhs.benchmark_name < rhs.benchmark_name;
}

bool has_runtime_values(const benchmark_file& bf) {
  return bf.type == runtime_values;
}

bool is_invalid_file(const benchmark_file& bf) {
  return bf.type == invalid_file;
}

constexpr char newline = '\n';

constexpr char yerr_suffix[] = "_yerr";

constexpr int yerr_suffix_size = 5;

constexpr char file_name_default_format[] =
    "{X-VALUE}_{X-LABEL}_{MEMORY_OR_RUNTIME}_{LABEL}_{BENCHMARK}\\.txt";

constexpr size_t num_field_names = 5;

constexpr const char* field_names [num_field_names] = {
  "X-VALUE",
  "X-LABEL",
  "MEMORY_OR_RUNTIME",
  "LABEL",
  "BENCHMARK"
};

void print_help(int exit_code) {
  cout << "to_csv [-f FORMAT] FILES..." << endl
       << "default format string: " << file_name_default_format << endl;
  exit(exit_code);
}

using cstr_iterator = const char*;

int index_of(const char* cstr,
             const vector<std::pair<cstr_iterator, cstr_iterator>>& vec) {
  for (size_t i = 0; i < vec.size(); ++i) {
    auto x = vec[i];
    if (distance(x.first, x.second) == static_cast<ptrdiff_t>(strlen(cstr))
        && equal(x.first, x.second, cstr))
      return static_cast<int>(i);
  }
  return -1;
}

pair<regex, map<string, size_t>> read_format(const char* format_str) {
  map<string, size_t> mapping;
  auto first = format_str;
  auto last = first + strlen(format_str);
  vector<std::pair<cstr_iterator, cstr_iterator>> vars;
  // collect all variables with syntax "{...}"
  auto i = find(first, last, '{');
  auto e = find(i, last, '}');
  while (i != e && e != last) {
    vars.push_back(make_pair(i + 1, e));
    i = find(e, last, '{');
    e = find(i, last, '}');
  }
  if (vars.size() != num_field_names) {
    cerr << "expected " << num_field_names << " field names, found "
         << vars.size() << endl;
    print_help(1);
  }
  for (auto fn : field_names) {
    // get index for this field
    auto ix = index_of(fn, vars);
    if (ix == -1) {
      cerr << "field missing: " << fn << endl;
      print_help(1);
    }
    mapping.emplace(fn, ix + 1);
  }
  // replace all field names in format string
  using caf::replace_all;
  std::string fstr = format_str;
  replace_all(fstr, "{X-VALUE}", "([0-9]+)");
  replace_all(fstr, "{X-LABEL}", "([a-zA-Z_\\-]+)");
  replace_all(fstr, "{MEMORY_OR_RUNTIME}", "(runtime|memory_[0-9]+)");
  replace_all(fstr, "{LABEL}", "([a-zA-Z0-9\\-]+)");
  replace_all(fstr, "{BENCHMARK}", "([a-zA-Z_\\-]+)");
  regex rx{fstr};
  return make_pair(std::move(rx), std::move(mapping));
}

class application {
 public:
  application(pair<regex, map<string, size_t>> field_conf)
      : m_nice_names{{"caf",     "CAF"},
                     {"scala",   "Scala"},
                     {"salsa",   "SalsaLite"},
                     {"theron",  "Theron"},
                     {"go",      "GoLang"},
                     {"charm",   "Charm"},
                     {"foundry", "ActorFoundry"},
                     {"erlang",  "Erlang"},
                     {"mpi",     "MPI"}},
        m_fname_rx{std::move(field_conf.first)},
        m_fname_ids{std::move(field_conf.second)},
        m_field_width{0} {
    for (auto& nn : m_nice_names) {
      m_field_width = max(m_field_width, static_cast<int>(nn.second.size())
                                         + yerr_suffix_size);
    }
    m_empty_field.assign(static_cast<size_t>(m_field_width), ' ');
  }
  using iterator = vector<benchmark_file>::iterator;
  void run(vector<string> fnames) {
    // parse file names and remove invalid files
    auto parse_fname = [&](string& fname) -> benchmark_file {
      benchmark_file res;
      smatch rxres;
      if (regex_match(fname, rxres, m_fname_rx) && rxres.size() == 6) {
        res.num_units = stoul(rxres.str(m_fname_ids["X-VALUE"]));
        m_unit_name = rxres.str(m_fname_ids["X-LABEL"]);
        res.type = rxres.str(m_fname_ids["MEMORY_OR_RUNTIME"]) == "runtime"
                             ? runtime_values
                             : memory_values;
        res.framework = rxres.str(m_fname_ids["LABEL"]);
        res.benchmark_name = rxres.str(m_fname_ids["BENCHMARK"]);
        res.path = std::move(fname);
      } else {
        res.type = invalid_file;
        cerr << "*** file name \"" << fname
             << "\" does not match regex" << endl;
      }
      return res;
    };
    vector<benchmark_file> files;
    transform(fnames.begin(), fnames.end(), back_inserter(files), parse_fname);
    files.erase(remove_if(files.begin(), files.end(), is_invalid_file),
                files.end());
    // get the names of our benchmarks
    auto bench_name = [](const benchmark_file& bf) {
      return bf.benchmark_name;
    };
    vector<string> benchs;
    transform(files.begin(), files.end(), back_inserter(benchs), bench_name);
    sort(benchs.begin(), benchs.end());
    benchs.erase(unique(benchs.begin(), benchs.end()), benchs.end());
    m_benchmarks.swap(benchs);
    // separate
    auto first_mem = partition(files.begin(), files.end(), has_runtime_values);
    // sort subranges by benchmark name
    sort(files.begin(), first_mem, bench_name_cmp);
    sort(first_mem, files.end(), bench_name_cmp);
    // convert runtime and memory files
    convert_runtime_files(files.begin(), first_mem);
    convert_mem_files(first_mem, files.end());
  }

 private:
  void convert_runtime_files(iterator first, iterator last) {
    if (first == last) {
      return;
    }
    iterator eor;
    { // lifetime scope of local variables
      // find end of this range
      auto benchmark_name = first->benchmark_name;
      auto different_benchmark = [&](const benchmark_file& bf) {
        return bf.benchmark_name != benchmark_name;
      };
      eor = find_if(first, last, different_benchmark); // end-of-range
      // $framework => {$num_units => [$values]}
      map<string, map<size_t, vector<double>>> samples;
      for (; first != eor; ++first) {
        auto vals = content(first->path, 1);
        if (vals.empty()) {
          cerr << "*** no values found in " << first->path << endl;
        } else {
          auto& out = samples[first->framework][first->num_units];
          for (auto& row : vals) {
            out.push_back(row[0]);
          }
        }
      }
      // compute statistics and print result for this range
      // calculate filed width from maximum field name + "_yerr"
      ostringstream tmp;
      tmp << left;
      tmp << setw(m_field_width) << m_unit_name;
      map<size_t, map<string, pair<double, double>>> output_table;
      auto no_nice_name = m_nice_names.end();
      for (auto& kvp : samples) {
        auto& framework = kvp.first;
        auto iter = m_nice_names.find(framework);
        auto& out_name = (iter == no_nice_name) ? framework : iter->second;
        tmp << ", " << setw(m_field_width) << out_name
            << ", " << setw(m_field_width) << (out_name + yerr_suffix);
        for (auto& kvp2 : kvp.second) {
          auto num_units = kvp2.first;
          statistics stats{kvp2.second};
          auto yerr = kvp2.second.size() < 9 ? 0. : stats.conf_interval_95;
          output_table[num_units][framework] = make_pair(stats.mean, yerr);
        }
      }
      auto ofile_header = tmp.str();
      // trime trailing whitespaces
      ofile_header.erase(ofile_header.find_last_not_of(' ') + 1);
      ofstream ofile{benchmark_name + ".csv"};
      ofile << left;
      ofile << ofile_header << newline;
      for (auto& output_kvp : output_table) {
        ofile << setw(m_field_width) << output_kvp.first; // number of units
        auto end_i = output_kvp.second.end();
        auto last_i = end_i;
        --last_i;
        for (auto i = output_kvp.second.begin(); i != end_i; ++i) {
          // print mean and 95% confidence interval
          ofile << ", " << setw(m_field_width) << i->second.first << ", ";
          // supress trailing whitespaces
          if (i != last_i) {
            ofile << setw(m_field_width);
          }
          ofile << i->second.second;
        }
        ofile << newline;
      }
    }
    // recursive call to next benchmark (after locals are cleaned up)
    convert_runtime_files(eor, last);
  }

  void convert_mem_files(iterator first, iterator last) {
    if (first == last) {
      return;
    }
    iterator eor;
    { // lifetime scope of local variables
      // find end of this range
      auto benchmark_name = first->benchmark_name;
      auto different_benchmark = [&](const benchmark_file& bf) {
        return bf.benchmark_name != benchmark_name;
      };
      eor = find_if(first, last, different_benchmark); // end-of-range
      // $framework => [$values]
      map<string, vector<double>> samples;
      for (; first != eor; ++first) {
        auto vals = content(first->path, 2);
        if (!vals.empty()) {
          auto& out = samples[first->framework];
          for (auto& row : vals) {
            out.push_back(row[1]);
          }
        }
      }
      // calculate filed width from maximum field name + "_yerr"
      ostringstream tmp;
      tmp << left;
      size_t cols = 0;
      bool at_begin = true;
      auto no_nice_name = m_nice_names.end();
      for (auto& kvp : samples) {
        auto& framework = kvp.first;
        auto iter = m_nice_names.find(framework);
        auto& nice_name = (iter == no_nice_name) ? framework : iter->second;
        if (!at_begin) {
          tmp << ",";
        } else {
          at_begin = false;
        }
        tmp << nice_name;
        cols = max(cols, kvp.second.size());
      }
      auto ofile_header = tmp.str();
      ofstream ofile{"memory_" + benchmark_name + ".csv"};
      ofile << left;
      ofile << ofile_header << newline;
      for (size_t col = 0; col < cols; ++col) {
        auto iter = samples.begin();
        for (size_t row = 0; row < samples.size(); ++row) {
          if (row > 0) {
            ofile << ",";
          }
          if (col < iter->second.size()) {
            ofile << iter->second[col];
          }
          ++iter;
        }
        ofile << newline;
      }
    }
    // recursive call to next benchmark (after locals are cleaned up)
    convert_mem_files(eor, last);
  }

  vector<vector<double>> content(const file_name& fname, size_t row_size) {
    vector<vector<double>> result;
    string line;
    ifstream f{fname};
    istream_iterator<double> eos; // end-of-stream iterator
    while (getline(f, line)) {
      vector<double> values;
      istringstream iss{line};
      for (istream_iterator<double> i{iss}; i != eos; ++i) {
        values.push_back(*i);
      }
      if (!values.empty()) {
        result.push_back(move(values));
      }
    }
    auto pred = [=](const vector<double>& v) { return v.size() == row_size; };
    if (!result.empty() && all_of(result.begin(), result.end(), pred)) {
      return result;
    }
    cerr << "*** invalid or empty file: " << fname << endl;
    return {};
  }

  map<string, string> m_nice_names;
  regex m_fname_rx;
  map<string, size_t> m_fname_ids;
  vector<string> m_benchmarks;
  int m_field_width;
  string m_empty_field;
  string m_unit_name; // usually either "cores" or "machines"
};

int main(int argc, char** argv) {
  int offset = 0;
  pair<regex, map<string, size_t>> format_config;
  if (argc >= 3 && strcmp(argv[1], "-f") == 0) {
    offset = 2;
    format_config = read_format(argv[2]);
  } else {
    format_config = read_format(file_name_default_format);
  }
  application app{std::move(format_config)};
  app.run({argv + 1 + offset, argv + argc});
}
