//
// Created by sahil on 07/05/23.
//

#ifndef DEHAZE_FUNCTIMER_H
#define DEHAZE_FUNCTIMER_H

#include <chrono>
#include <utility>


// Get time unit to be used
template <typename TimeUnit>
class functimer {
public:
    template<typename F, typename... Args>
    auto funcTime(F func, Args&&... args){
        auto start = std::chrono::steady_clock::now();
        func(std::forward<Args>(args)...);
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<TimeUnit>(end - start).count();
    }

};


#endif //DEHAZE_FUNCTIMER_H
