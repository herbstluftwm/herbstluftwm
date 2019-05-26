#include "monitordetection.h"

#include "xconnection.h"

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

using std::string;

MonitorDetection::MonitorDetection(string name)
    : name_(name)
    , detect_(nullptr)
{
}

#ifdef XINERAMA

// inspired by dwm's updategeom()
RectangleVec detectMonitorsXinerama(XConnection& X) {
    if (!XineramaIsActive(X.display())) {
        return {};
    }
    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(X.display(), &n);
    RectangleVec monitor_rects;
    for (int i = 0; i < n; i++) {
        Rectangle r;
        r.x = info[i].x_org;
        r.y = info[i].y_org;
        r.width = info[i].width;
        r.height = info[i].height;
        monitor_rects.push_back(r);
    }
    XFree(info);
    return monitor_rects;
}

#endif /* XINERAMA */

vector<MonitorDetection> MonitorDetection::detectors() {
    MonitorDetection xinerama("xinerama");
    #ifdef XINERAMA
    xinerama.detect_ = detectMonitorsXinerama;
    #endif
    return { xinerama };
}

