#include "monitordetection.h"

#include "globals.h"
#include "xconnection.h"

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include <X11/extensions/Xrandr.h>

using std::make_pair;
using std::string;
using std::vector;

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
    int n = 0;
    XineramaScreenInfo* info = XineramaQueryScreens(X.display(), &n);
    if (n == 0 || !info) {
        return {};
    }
    RectangleVec monitor_rects;
    for (int i = 0; i < n; i++) {
        Rectangle r = {};
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

RectangleVec detectMonitorsXrandr(XConnection& X) {
    // see /usr/share/doc/xorgproto/randrproto.txt for documentation
    int outputs = 0;
    int event_base = 0;
    int error_base = 0;
    if (!XRRQueryExtension(X.display(), &event_base, &error_base)) {
        HSDebug("no xrandr available\n");
        return {};
    }

    int major_version = 0, minor_version = 0;
    XRRQueryVersion(X.display(), &major_version, &minor_version);
    if (make_pair(major_version, minor_version) < make_pair(1, 5)) {
        HSDebug("RRGetMonitors only available since RandR 1.5");
        return {};
    }
    // RRGetMonitors was added with randr 1.5
    XRRMonitorInfo* monitorInfo = XRRGetMonitors(X.display(), X.root(), true, &outputs);
    if (outputs <= 0 || !monitorInfo) {
        return {};
    }
    RectangleVec result;
    for (int i = 0; i < outputs; i++) {
        XRRMonitorInfo& cur = monitorInfo[i];
        result.push_back({ cur.x, cur.y, cur.width, cur.height });
    }
    XRRFreeMonitors(monitorInfo);
    return result;
}

vector<MonitorDetection> MonitorDetection::detectors() {
    MonitorDetection xrandr("xrandr");
    xrandr.detect_ = detectMonitorsXrandr;
    MonitorDetection xinerama("xinerama");
    #ifdef XINERAMA
    xinerama.detect_ = detectMonitorsXinerama;
    #endif
    return { xrandr, xinerama };
}

