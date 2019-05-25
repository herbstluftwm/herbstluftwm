#pragma once

#include <string>
#include <vector>

#include "x11-types.h"

class XConnection;

class MonitorDetection {
public:
    MonitorDetection(std::string name);
    std::string name_;
    /** whether this is supported by the X display.
     * This pointer is null if the monitor detection is deactivated at compile time
     */
    bool (*checkDisplay_)(XConnection& X);
    //! run the detection
    RectangleVec (*detect_)(XConnection& X);

    static MonitorDetection xinerama();
};

