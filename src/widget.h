#ifndef WIDGET_H
#define WIDGET_H

#include "css.h"
#include "rectangle.h"

#include <vector>
#include <memory>

class BoxStyle;
class X11WidgetRender;

class Widget : public DomTree
{
public:
    Widget();
    virtual ~Widget() override = default;
    /** whether the children of this widged are placed below each other */
    bool vertical_ = false;
    /** whether this widget likes growing into the X-direction */
    bool expandX_ = false;
    /** whether this widget likes growing into the Y-direction */
    bool expandY_ = false;
    bool hasText_ = false;
    virtual std::string textContent() const { return {}; }

    void computeGeometry(Rectangle outerGeometry);
    void computeMinimumSize();
    Point2D minimumSizeCached() const {
        return minimumSizeCached_;
    }
    Rectangle geometryCached() const {
        return geometryCached_;
    }
    Rectangle contentGeometryCached() const;
    void moveGeometryCached(Point2D delta);
    void clearChildren();
    void addChild(Widget* child);
    void removeChild(size_t idx);
    void setStyle(std::shared_ptr<BoxStyle> style);
    bool isDisplayNone() const;

    Point2D minimumSizeUser_ = {0, 0}; //! custom minimum size

    void recurse(std::function<void(Widget&)> body);

    const DomTree* parent() const override;
    const DomTree* nthChild(size_t idx) const override;
    const DomTree* leftSibling() const override;
    bool hasClass(const CssName& className) const override;
    size_t childCount() const override;
    void setClasses(const CssNameSet& classes);
    void setClassEnabled(const CssName& className, bool enabled);
    void setClassEnabled(std::initializer_list<std::pair<CssName, bool>> classes);
private:
    friend class X11WidgetRender;
    CssNameSet classes_ = {};
    Widget* parent_ = nullptr;
    size_t indexInParent_ = 0;
    std::vector<Widget*> nestedWidgets_;
    std::shared_ptr<const BoxStyle> style_;
    Rectangle geometryCached_;
    Point2D minimumSizeCached_ = {0, 0};
};

#endif // WIDGET_H
