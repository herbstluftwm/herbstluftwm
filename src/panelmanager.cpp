#include "panelmanager.h"

#include "settings.h"
#include "x11-types.h"
#include "xconnection.h"

using std::make_pair;
using std::string;
using std::vector;

class Panel {
public:
    Panel(Window winid, PanelManager& pm) : winid_(winid), pm_(pm) {}
    Window winid_;
    PanelManager& pm_;
    Rectangle size_;
    vector<long> wmStrut_ = {};
    int operator[](PanelManager::WmStrut idx) const {
        size_t i = static_cast<size_t>(idx);
        if (i < wmStrut_.size()) {
            return static_cast<int>(wmStrut_[i]);
        } else if (idx == PanelManager::WmStrut::left_end_y
               || idx == PanelManager::WmStrut::right_end_y)
        {
            return pm_.rootWindowGeometry_.height;
        } else if (idx == PanelManager::WmStrut::top_end_x
               || idx == PanelManager::WmStrut::bottom_end_x)
        {
            return pm_.rootWindowGeometry_.width;
        } else {
            return 0;
        }
    }
};

PanelManager::PanelManager(XConnection& xcon)
    : xcon_(xcon)
{
    atomWmStrut_ = xcon_.atom("_NET_WM_STRUT");
    atomWmStrutPartial_ = xcon_.atom("_NET_WM_STRUT_PARTIAL");
    rootWindowGeometry_ = xcon_.windowSize(xcon_.root());
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
}

/**
 * read the reserved space from the panel window and return if there are changes
 * - size is the geometry of the panel
 */
bool PanelManager::updateReservedSpace(Panel* p, Rectangle size)
{
    auto optionalWmStrut = xcon_.getWindowPropertyCardinal(p->winid_, atomWmStrut_);
    if (!optionalWmStrut) {
        optionalWmStrut= xcon_.getWindowPropertyCardinal(p->winid_, atomWmStrutPartial_);
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
        Rectangle intersection = mon.intersectionWith(p.size_);
        if (!intersection) {
            // monitor does not intersect with panel at all
            continue;
        }
        // we only reserve space for the panel if the panel defines
        // wmStrut_ or if the aspect ratio clearly indicates whether the
        // panel is horizontal or vertical
        if (p.wmStrut_.empty()) {
            // if the panel does not define wmStrut, then
            // try to detect it automatically from the intersection
            if (intersection.height > intersection.width) {
                // vertical panels
                if (intersection.x == mon.x) {
                    rs.left_ = intersection.width;
                }
                if (intersection.br().x == mon.br().x) {
                    rs.right_ = intersection.width;
                }
            }
            if (intersection.height < intersection.width) {
                // horizontal panels
                if (intersection.y == mon.y) {
                    rs.top_ = intersection.height;
                }
                if (intersection.br().y == mon.br().y) {
                    rs.bottom_ = intersection.height;
                }
            }
        } else {
            // if the panel explicitly defines wmStrut
            // then simply use this
            rs.left_ = p[WmStrut::left];
            rs.right_ = p[WmStrut::right];
            rs.top_ = p[WmStrut::top];
            rs.bottom_ = p[WmStrut::bottom];
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
