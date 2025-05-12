/*
    Implementation of SVM Scheduler
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <queue>

#include "scheduler.hpp"

enum Status {
    FirstAppear, Miss, Hit
};

class OPTObject {
    
};

class OPTgen {
public:
    uint64_t CacheSize;
    std::vector<uint64_t> Capacity;
    std::map<uint64_t, int64_t> LastVisitedTime;
    int64_t TimeStamp;

    OPTgen(uint64_t cache_size) : CacheSize(cache_size), TimeStamp(-1) {
        Capacity = std::vector<uint64_t>();
        LastVisitedTime = std::map<uint64_t, int64_t>();
    }

    Status TimeStampInc(uint64_t obj_id) {
        ++TimeStamp;
        Capacity.push_back(0);
        assert(Capacity.size() == TimeStamp);
        Status ret;
        if (LastVisitedTime.find(obj_id) == LastVisitedTime.end())
            ret = FirstAppear;
        else {
            ret = Hit;
            for (auto i = LastVisitedTime[obj_id]; i <= TimeStamp; i++) {
                if (Capacity[i] == CacheSize) {
                    ret = Miss;
                    break;
                }
            }
        }
        if (ret == Hit) {
            for (auto i = LastVisitedTime[obj_id]; i <= TimeStamp; i++)
                Capacity[i]++;
        }
        LastVisitedTime[obj_id] = TimeStamp;
        return ret;
    }
};

class SVMObject {
public:
    bool IsInCache;
    uint64_t obj_id;
    uint64_t LastVisitedTime;
    int64_t CacheFriend;
    std::map<uint64_t, int64_t> weights;

    SVMObject(uint64_t obj_id, uint64_t LastVisitedTime, int64_t CacheFriend) : 
        IsInCache(false), obj_id(obj_id), LastVisitedTime(LastVisitedTime), CacheFriend(CacheFriend) {
        weights = std::map<uint64_t, int64_t>();
    }

    SVMObject(bool IsInCache, uint64_t obj_id, uint64_t LastVisitedTime, int64_t CacheFriend) : 
        IsInCache(IsInCache), obj_id(obj_id), LastVisitedTime(LastVisitedTime), CacheFriend(CacheFriend) {
        weights = std::map<uint64_t, int64_t>();
    }
};

class PCHR {
public:
    static const uint64_t PCHRSize = 5;
    std::vector<uint64_t> elements;

    PCHR() {
        elements = std::vector<uint64_t>();
    }

    void Insert(uint64_t obj_id) {
        for (auto it = elements.begin(); it != elements.end(); it++)
            if (*it == obj_id) {
                elements.erase(it);
                break;
            }
        elements.insert(elements.begin(), obj_id);
        if (elements.size() > PCHRSize)
            elements.pop_back();
    }
};

class CacheObject {
public:
    uint64_t obj_id;
    uint64_t LastVisitedTime;
    int64_t CacheFriend;

    CacheObject(uint64_t obj_id, uint64_t LastVisitedTime,int64_t CacheFriend) : 
        obj_id(obj_id), LastVisitedTime(LastVisitedTime), CacheFriend(CacheFriend) {

    }

    bool operator < (const CacheObject rhs) const {
        if (CacheFriend != rhs.CacheFriend)
            return CacheFriend < rhs.CacheFriend;
        else {
            if (LastVisitedTime != rhs.LastVisitedTime)
                return LastVisitedTime < rhs.LastVisitedTime;
            return obj_id < rhs.obj_id;
        }
    }
};

class SVMScheduler : public Scheduler {
public:
    OPTgen *Gen;
    PCHR *PCRegister;
    static const int64_t TrainingThreshold = 30;
    static const int64_t PredictionThreshold = 60;
    static const int64_t LearningRate = 1;
    
    std::set<CacheObject> Cache;
    std::map<uint64_t, SVMObject> ObjInfo;

    SVMScheduler(uint64_t cache_size) : Scheduler(cache_size) {
        Gen = new OPTgen(cache_size);
        PCRegister = new PCHR();
        Cache = std::set<CacheObject>();
        ObjInfo = std::map<uint64_t, SVMObject>();
    }

    ~SVMScheduler() {
        delete Gen;
        delete PCRegister;
    }

    bool CheckInCache(uint64_t obj_id) {
        auto it = ObjInfo.find(obj_id);
        if (it != ObjInfo.end())
            return it -> second.IsInCache;
        return false;
    }

    /*
    Insert and delete should change Cache and ObjInfo
    */
    void Insert(uint64_t obj_id, uint64_t LastVisitedTime, int64_t CacheFriend) {
        auto it = ObjInfo.find(obj_id);
        if (it != ObjInfo.end() && it -> second.IsInCache) {
            auto Cache_it = Cache.find(CacheObject(obj_id, it -> second.LastVisitedTime, it -> second.CacheFriend));
            Cache.erase(Cache_it);
            Cache.insert(CacheObject(obj_id, LastVisitedTime, CacheFriend));
            it -> second.CacheFriend = CacheFriend;
            it -> second.LastVisitedTime = LastVisitedTime;
            it -> second.IsInCache = true;
        } else {
            if (Cache.size() == this -> cache_size)
                Delete();
            Cache.insert(CacheObject(obj_id, LastVisitedTime, CacheFriend));
            auto it = ObjInfo.find(obj_id);
            if (it != ObjInfo.end()){
                it -> second.CacheFriend = CacheFriend;
                it -> second.LastVisitedTime = LastVisitedTime;
                it -> second.IsInCache = true;
            } else {
                ObjInfo.emplace(obj_id, SVMObject(true, obj_id, LastVisitedTime, CacheFriend));
            }
        }
    }

    void Delete() {
        auto it = Cache.begin();
        auto it1 = ObjInfo.find(it -> obj_id);
        it1 -> second.IsInCache = false;
        Cache.erase(it);
    }

    Result run(std::vector<Request>& requests) {
        auto result = Result(requests);

        for (auto &request : requests) {
            auto obj_id = request.obj_id;
            auto status = Gen -> TimeStampInc(obj_id);
            auto TimeStamp = Gen -> TimeStamp;
            auto IsInCache = CheckInCache(obj_id);
            if (!IsInCache)
                result.cache_misses++;
            int64_t prediction = 0;
            auto it1 = ObjInfo.find(obj_id);
            if (it1 != ObjInfo.end()) {
                for (auto v = PCRegister -> elements.begin();
                    v != PCRegister -> elements.end(); v++) {
                    auto it = it1 -> second.weights.find(*v);
                    auto NotBlank = (it != it1 -> second.weights.end());
                    if (NotBlank) {
                        prediction += it->second;
                    }
                    if (status == Hit) {
                        if (!NotBlank) {
                            it1 -> second.weights.emplace(*v, 0);
                            it = it1 -> second.weights.find(*v);
                        }
                        if (it -> second <= TrainingThreshold)
                            it -> second += LearningRate;
                    }
                    else {
                        if (!NotBlank) {
                            it1 -> second.weights.emplace(*v, 0);
                            it = it1 -> second.weights.find(*v);
                        }
                        if (it -> second >= -TrainingThreshold)
                            it -> second -= LearningRate;
                    }
                }
            }
            Insert(obj_id, TimeStamp, prediction);
            PCRegister -> Insert(obj_id);
        }
        return result;
    }
};

namespace py = pybind11;

PYBIND11_MODULE(svm, m) {
    // Bind the SVM scheduler
    py::class_<SVMScheduler>(m, "SVM")
        .def(py::init<const uint64_t>())
        .def("run", &SVMScheduler::run);
}