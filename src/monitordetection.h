#pragma once

#include <string>
#include <vector>

#include "rectangle.h"

class XConnection;

class MonitorDetection {
public:
    MonitorDetection(std::string name);
    std::string name_;
    /** run the monitor detection.
     * This pointer is null if the monitor detection is deactivated at compile time
     */
    RectangleVec (*detect_)(XConnection& X);

    static std::vector<MonitorDetection> detectors();
};

