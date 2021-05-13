#include "lpf.h"

void lpf_update_estimate(lpf_data_t * current, int32_t * measure)
{
    // Old = Old - Gain*(Old - new)     Gain : percent
    current->estimate_value = current->estimate_value - current->gain*(current->estimate_value - *measure);
}
