#include "native_bridge.hpp"

double sagan_5f5f746573745f6e61746976655f64697374616e6365(double value, bool enabled) {
    return enabled ? value : 0.0;
}
