#include "panelmanager.h"

#include "settings.h"
#include "x11-types.h"
#include "xconnection.h"

using std::make_pair;
using std::string;
using std::vector;

class Panel : public Object {
public:
    Panel(Window winid, PanelManager& pm)
        : winid_(this, "winid", winid)
        , windowInstance_(this, "instance", {})
        , windowClass_(this, "class", {})
        , pm_(pm)
        , size_(this, "geometry", {})
    {
        auto hint = pm.xcon_.getClassHint(winid);
        windowInstance_ = hint.first;
        windowClass_ = hint.second;
        winid_.setDoc("the ID of the panel window");
        windowInstance_.setDoc("the window instance (first entry of WM_CLASS)");
        windowClass_.setDoc("the window class (second entry of WM_CLASS)");
        size_.setDoc("the size and position of the window");
        setDoc("a panel is an unmanaged window that reserves space at "
               "the edge of the monitor it is on. The space depends on "
               "the _NET_WM_STRUT defined by the panel. If it is however "
               "not defined explicitly, then the amount of reserved space "
               "is inferred from the window geometry.");
    }
    Attribute_<WindowID> winid_;
    Attribute_<string> windowInstance_;
    Attribute_<string> windowClass_;
    PanelManager& pm_;
    Attribute_<Rectangle> size_;
    vector<long> wmStrut_ = {};
    using WmStrut = PanelManager::WmStrut;

    int wmStrut(WmStrut idx) const {
        size_t i = static_cast<size_t>(idx);
        if (i < wmStrut_.size()) {
            return static_cast<int>(wmStrut_[i]);
        } else if (idx == WmStrut::left_end_y
               || idx == WmStrut::right_end_y)
        {
            return pm_.rootWindowGeometry_.height;
        } else if (idx == WmStrut::top_end_x
               || idx == WmStrut::bottom_end_x)
        {
            return pm_.rootWindowGeometry_.width;
        } else {
            return 0;
        }
    }

    /** report the panels geometry based on WM_STRUT
     * The returned rectangle is guaranteed to touch
     * an edge of the root window rectangle.
     */
    Rectangle wmStrutGeometry() const {
        if (wmStrut(WmStrut::top) > 0) {
            // align with top edge
            return Rectangle::fromCorners(
                        wmStrut(WmStrut::top_start_x),
                        0,
                        wmStrut(WmStrut::top_end_x),
                        wmStrut(WmStrut::top));
        }
        if (wmStrut(WmStrut::bottom) > 0) {
            // align with bottom edge
            return Rectangle::fromCorners(
                        wmStrut(WmStrut::bottom_start_x),
                        pm_.rootWindowGeometry_.height - wmStrut(WmStrut::bottom),
                        wmStrut(WmStrut::bottom_end_x),
                        pm_.rootWindowGeometry_.height);
        }
        if (wmStrut(WmStrut::left) > 0) {
            // align with left edge
            return Rectangle::fromCorners(
                        0,
                        wmStrut(WmStrut::left_start_y),
                        wmStrut(WmStrut::left),
                        wmStrut(WmStrut::left_end_y));
        }
        if (wmStrut(WmStrut::right) > 0) {
            // align with right edge
            return Rectangle::fromCorners(
                        pm_.rootWindowGeometry_.width - wmStrut(WmStrut::right),
                        wmStrut(WmStrut::right_start_y),
                        pm_.rootWindowGeometry_.width,
                        wmStrut(WmStrut::right_end_y));
        }
        return {0, 0, 0, 0};
    };
};

PanelManager::PanelManager(XConnection& xcon)
    : count(this, "count", &PanelManager::getCount)
    , geometryFallback_(this, "geometry_fallback", false)
    , xcon_(xcon)
{
    geometryFallback_.setWritable(true);
    atomWmStrut_ = xcon_.atom("_NET_WM_STRUT");
    atomWmStrutPartial_ = xcon_.atom("_NET_WM_STRUT_PARTIAL");
    rootWindowGeometry_ = xcon_.windowSize(xcon_.root());
    setDoc("For every panel window, there is an entry with "
           "the panel's window id here.");
    geometryFallback_.setDoc("when auto-detecting panels: "
                             "if a panel does not specify the amount of space it needs "
                             "at the monitor edge, fall back to reserve the panel\'s geometry"
                             "at the screen edge.");
    geometryFallback_.changed().connect(panels_changed_);
}

PanelManager::~PanelManager()
{
    for (auto it : panels_) {
        delete it.second;
    }
}

void PanelManager::registerPanel(Window win)
{
    Panel* p = new Panel(win, *this);
    panels_.insert(make_pair(win, p));
    addChild(p, Converter<WindowID>::str(win));
    updateReservedSpace(p, xcon_.windowSize(win));
    panels_changed_.emit();
}

void PanelManager::unregisterPanel(Window win)
{
    auto it = panels_.find(win);
    if (it == panels_.end()) {
        return;
    }
    Panel* p = it->second;
    panels_.erase(win);
    removeChild(Converter<WindowID>::str(win));
    delete p;
    panels_changed_.emit();
}

void PanelManager::propertyChanged(Window win, Atom property)
{
    if (property != atomWmStrut_ && property != atomWmStrutPartial_) {
        return;
    }
    auto it = panels_.find(win);
    if (it != panels_.end()) {
        Panel* p = it->second;
        if (updateReservedSpace(p, xcon_.windowSize(win))) {
            panels_changed_.emit();
        }
    }
}

/**
 * @brief the geometry of a window was changed, where window
 * is possibly a panel window
 * @param the window
 * @param its new geometry
 */
void PanelManager::geometryChanged(Window win, Rectangle geometry)
{
    auto it = panels_.find(win);
    if (it != panels_.end()) {
        Panel* p = it->second;
        if (updateReservedSpace(p, geometry)) {
            panels_changed_.emit();
        }
    }
}

void PanelManager::injectDependencies(Settings* settings)
{
    settings_ = settings;
    settings_->auto_detect_panels.changed().connect([this]() {
        panels_changed_.emit();
    });
}

/**
 * read the reserved space from the panel window and return if there are changes
 * - size is the geometry of the panel
 */
bool PanelManager::updateReservedSpace(Panel* p, Rectangle size)
{
    auto optionalWmStrut = xcon_.getWindowPropertyCardinal(p->winid_(), atomWmStrutPartial_);
    if (!optionalWmStrut) {
        optionalWmStrut= xcon_.getWindowPropertyCardinal(p->winid_(), atomWmStrut_);
    }
    vector<long> wmStrut = optionalWmStrut.value_or(vector<long>());
    if (p->wmStrut_ != wmStrut || p->size_ != size) {
        p->wmStrut_ = wmStrut;
        p->size_ = size;
        return true;
    }
    return false;
}


//! given the dimension of a monitor, return the space reserved for panels
PanelManager::ReservedSpace PanelManager::computeReservedSpace(Rectangle mon)
{
    ReservedSpace rsTotal;
    if (!settings_->auto_detect_panels()) {
        return rsTotal;
    }
    for (auto it : panels_) {
        Panel& p = *(it.second);
        ReservedSpace rs;
        Rectangle panelArea = p.wmStrutGeometry();
        if (!panelArea) {
            // if the panel does not define WmStrut,
            // then take it's window geometry
            panelArea = p.size_;
        }
        Rectangle intersection = mon.intersectionWith(panelArea);
        if (!intersection) {
            // monitor does not intersect with panel at all
            continue;
        }
        if (p.wmStrut_.empty() && !geometryFallback_) {
            // if a panel does not define WM_STRUT, then we ignore it
            // unless we are allowed to fall back to the panel's geometry
            continue;
        }
        // we only reserve space for the panel if the panel defines
        // wmStrut_ or if the aspect ratio clearly indicates whether the
        // panel is horizontal or vertical
        bool verticalPanel = p.wmStrut(WmStrut::left) > 0 || p.wmStrut(WmStrut::right) > 0;
        bool horizontalPanel = p.wmStrut(WmStrut::top) > 0 || p.wmStrut(WmStrut::bottom) > 0;
        if (p.wmStrut_.empty()) {
            // only fall back to aspect ratio if wmStrut is undefined
            verticalPanel = intersection.height > intersection.width;
            horizontalPanel = intersection.height < intersection.width;
        }
        if (verticalPanel) {
            // don't affect the monitor, if the intersection spans
            // the entire monitor width.
            if (intersection.x == mon.x && intersection.width < mon.width) {
                rs.left_ = intersection.width;
            }
            if (intersection.br().x == mon.br().x && intersection.width < mon.width) {
                rs.right_ = intersection.width;
            }
        }
        if (horizontalPanel) {
            // don't affect the monitor, if the intersection spans
            // the entire monitor height.
            if (intersection.y == mon.y && intersection.height < mon.height) {
                rs.top_ = intersection.height;
            }
            if (intersection.br().y == mon.br().y && intersection.height < mon.height) {
                rs.bottom_ = intersection.height;
            }
        }
        for (size_t i = 0; i < 4; i++) {
            rsTotal[i] = std::max(rsTotal[i], rs[i]);
        }
    }
    return rsTotal;
}

void PanelManager::rootWindowChanged(int width, int height)
{
    rootWindowGeometry_.width = width;
    rootWindowGeometry_.height = height;
}

int& PanelManager::ReservedSpace::operator[](size_t idx)
{
    vector<int*> v = {  &left_, &right_, &top_, &bottom_ };
    return *(v[idx]);
}
