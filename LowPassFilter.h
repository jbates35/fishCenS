//
// Created by sahil on 05/05/23.
//

#ifndef DEHAZE_LOWPASSFILTER_H
#define DEHAZE_LOWPASSFILTER_H


class LowPassFilter {
public:
    LowPassFilter(float alpha) : alpha(alpha), prev_value(0) {}

    float filter(float value) {
        float filtered_value = alpha * value + (1 - alpha) * prev_value;
        prev_value = filtered_value;
        return filtered_value;
    }

private:
    float alpha; // smoothing factor
    float prev_value; // previous filtered value
};

#endif //DEHAZE_LOWPASSFILTER_H
