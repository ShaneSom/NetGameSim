#ifndef METRICS_HPP
#define METRICS_HPP

struct Metrics {
    long long messages_sent = 0;
    long long bytes_sent = 0;
    long long iterations = 0;
    double runtime_seconds = 0.0;
};

#endif