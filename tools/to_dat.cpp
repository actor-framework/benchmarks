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
    variance_plus(double mean) : mean(mean) { }
    double operator()(double res, double a) {
        auto tmp = mean - a;
        return res + (tmp * tmp);
    }
};

struct statistics  {

    double mean;
    double variance;
    double std_dev;
    double conf_interval_95;

    statistics(const vector<double>& data) {
        using namespace boost::math;
        mean = accumulate(data.begin(), data.end(), 0.0, plus<double>{}) / data.size();
        variance = accumulate(data.begin(), data.end(), 0.0, variance_plus{mean}) / data.size();
        std_dev = sqrt(variance);
        // calculate confidence interval
        students_t dist{data.size() - 1};
        // t-statistic for 95% confidence interval
        double tstat = quantile(complement(dist, 0.025 / 2));
        // width of confidence interval
        conf_interval_95 = tstat * std_dev / sqrt(static_cast<double>(data.size()));
    }

};

int main(int argc, char** argv) {
    vector<string> files(argv + 1, argv + argc);
    regex fname_regex("([0-9]+).*cores.*(cppa|scala|theron|erlang|go).*(actor_creation|mailbox_performance|mixed_case).txt");
    smatch fname_match;
    // $benchmark => {$lang => {$num_cores => [$values]}}
    map<string, map<string, map<size_t, vector<double>>>> samples;
    for (auto& fname : files) {
        if (regex_match(fname, fname_match, fname_regex) && fname_match.size() == 4) {
            istringstream iss(fname_match[1].str());
            size_t num_cores;
            iss >> num_cores;
            auto lang = fname_match[2].str();
            auto bench = fname_match[3].str();
            vector<double>& values = samples[bench][lang][num_cores];
            ifstream f{fname};
            while (f) {
                double value;
                f >> value;
                values.push_back(value);
            }
        }
        else {
            cerr << "file name \"" << fname << "\" does not match regex" << endl;
        }
    }
    if (samples.empty()) {
        cout << "... nothing to do ..." << endl;
        return 0;
    }
    for (auto& kvp : samples) {
        auto& benchmark = kvp.first;
        string file_header = "cores";
        map<size_t, map<string, pair<double, double>>> output_table;
        for (auto& kvp2 : kvp.second) {
            auto& lang = kvp2.first;
            file_header += " ";
            file_header += lang;
            for (auto& kvp3 : kvp2.second) {
                auto num_cores = kvp3.first;
                statistics stats{kvp3.second};
                output_table[num_cores][lang] = make_pair(stats.mean, stats.conf_interval_95);
            }
        }
        ofstream oss{benchmark + ".dat"};
        oss << file_header << endl;
        for (auto& output_kvp : output_table) {
            oss << output_kvp.first; // number of cores
            for (auto& inner_kvp : output_kvp.second) {
                oss << " " << inner_kvp.second.first   // mean
                     << " " << inner_kvp.second.second; // 95% confidence interval
            }
            oss << endl;
        }
    }
}
