#include "desktopwindow.h"

#include <algorithm>

#include "globals.h"

using std::function;
using std::make_shared;
using std::shared_ptr;
using std::vector;

vector<shared_ptr<DesktopWindow>> DesktopWindow::windows;

DesktopWindow::DesktopWindow(Window win, bool ifBelow)
    : win_(win)
    , below_(ifBelow)
{
}

Window DesktopWindow::window() const {
    return win_;
}

bool DesktopWindow::below() const {
    return below_;
}

void DesktopWindow::registerDesktop(Window win) {
    auto dw = make_shared<DesktopWindow>(win, true);
    windows.push_back(dw);
}

void DesktopWindow::unregisterDesktop(Window win) {
    windows.erase(std::remove_if(
                   windows.begin(), windows.end(),
                   [win](shared_ptr<DesktopWindow> dw){
                        return win == dw->window();
                   }),
                  windows.end());
}

void DesktopWindow::foreachDesktopWindow(function<void (DesktopWindow&)> loopbody)
{
    for (auto& dw : windows) {
        loopbody(*dw.get());
    }
}
