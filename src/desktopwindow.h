#ifndef __HERBSTLUFT_DESKTOPWINDOW_H_
#define __HERBSTLUFT_DESKTOPWINDOW_H_

#include <X11/X.h>
#include <memory>
#include <vector>

/* container for unmanaged windows like
 *  - desktop windows
 *  - panels
 */

class DesktopWindow {
public:
    DesktopWindow(Window win, bool ifBelow);
    bool below() const;
    Window window() const;
    static void registerDesktop(Window win);
    static void lowerDesktopWindows();
    static void unregisterDesktop(Window win);
private:

    // members:
    Window win_;
    bool below_;
    static std::vector<std::shared_ptr<DesktopWindow>> windows;

};

#endif

