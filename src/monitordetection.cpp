#include "monitordetection.h"

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#ifdef XINERAMA
// inspired by dwm's isuniquegeom()
static bool geom_unique(const RectangleVec& unique, XineramaScreenInfo *info) {
    for (const auto& u : unique) {
        if (u.x == info->x_org && u.y == info->y_org
        &&  u.width == info->width && u.height == info->height)
            return false;
    }
    return true;
}

// inspired by dwm's updategeom()
RectangleVec detectMonitorsXinerama(XConnection& X) {
    if (!XineramaIsActive(X.display())) {
        return {};
    }
    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(X.display(), &n);
    RectangleVec monitor_rects;
    for (int i = 0; i < n; i++) {
        if (geom_unique(monitor_rects, &info[i])) {
            Rectangle r;
            r.x = info[i].x_org;
            r.y = info[i].y_org;
            r.width = info[i].width;
            r.height = info[i].height;
            monitor_rects.push_back(r);
        }
    }
    XFree(info);
    return monitor_rects;
}
#else  /* XINERAMA */

RectangleVec detectMonitorsXinerama(XConnection& X) {
    return {};
}

#endif /* XINERAMA */

