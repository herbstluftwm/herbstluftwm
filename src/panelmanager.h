#pragma once

#include <X11/X.h>
#include <unordered_map>

#include "attribute_.h"
#include "object.h"
#include "rectangle.h"
#include "signal.h"

class Panel;
class Settings;
class XConnection;

class PanelManager : public Object {
public:
    class ReservedSpace {
    public:
        int left_ = 0;
        int right_ = 0;
        int top_ = 0;
        int bottom_ = 0;
        int& operator[](size_t idx);
    };
    //! the entries of  _NET_WM_STRUT_PARTIAL
    enum class WmStrut {
        left = 0,
        right,
        top,
        bottom,
        left_start_y, left_end_y,
        right_start_y, right_end_y,
        top_start_x, top_end_x,
        bottom_start_x, bottom_end_x,
    };

    PanelManager(XConnection& xcon);
    virtual ~PanelManager();
    void registerPanel(Window win);
    void unregisterPanel(Window win);
    void propertyChanged(Window win, Atom property);
    void geometryChanged(Window win, Rectangle size);
    void injectDependencies(Settings* settings);
    ReservedSpace computeReservedSpace(Rectangle monitorDimension);
    Signal panels_changed_;
    void rootWindowChanged(int width, int height);
    DynAttribute_<unsigned long> count;
private:
    friend Panel;
    unsigned long getCount() {
        return static_cast<unsigned long>(panels_.size());
    };
    bool updateReservedSpace(Panel* p, Rectangle geometry);

    std::unordered_map<Window, Panel*> panels_;
    Atom atomWmStrut_;
    Atom atomWmStrutPartial_;
    XConnection& xcon_;
    Rectangle rootWindowGeometry_;
    Settings* settings_ = nullptr;
};
