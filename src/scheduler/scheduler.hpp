#ifndef SCHEDULER_UTILS_HPP
#define SCHEDULER_UTILS_HPP

#include <vector>
#include <set>
#include "../cpp_utils.hpp"

class Result {
    public:
        uint64_t total_requests;
        uint64_t unique_pages;
        int cache_misses;

        Result(std :: vector<Request>& requests) {
            this -> total_requests = requests.size();

            std :: set<uint64_t> unique_pages;
            for (auto &request : requests) {
                unique_pages.insert(request.obj_id);
            }
            this -> unique_pages = unique_pages.size();

            this -> cache_misses = 0;
        }
};

class Scheduler {
    public:
        uint64_t cache_size;
        int64_t PredictionDivide;

        Scheduler(uint64_t cache_size, int64_t PredictionDivide) {
            this -> cache_size = cache_size;
            this -> PredictionDivide = PredictionDivide;
        }

        virtual Result run(std :: vector<Request>& requests) = 0;
};


#endif