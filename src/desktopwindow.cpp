#include "desktopwindow.h"
#include "globals.h"

#include <algorithm>

using namespace std;

std::vector<std::shared_ptr<DesktopWindow>> DesktopWindow::windows;

DesktopWindow::DesktopWindow(Window win, bool ifBelow) {
    m_below = ifBelow;
    this->win = win;
}

Window DesktopWindow::window() const {
    return win;
}

bool DesktopWindow::below() const {
    return m_below;
}

void DesktopWindow::registerDesktop(Window win) {
    auto dw = make_shared<DesktopWindow>(win, true);
    windows.push_back(dw);
}

void DesktopWindow::lowerDesktopWindows() {
    for (auto dw : windows) {
        XLowerWindow(g_display, dw->win);
    }
}

void DesktopWindow::unregisterDesktop(Window win) {
    windows.erase(std::remove_if(
                   windows.begin(), windows.end(),
                   [win](shared_ptr<DesktopWindow> dw){
                        return win == dw->window();
                   }),
                  windows.end());
}
