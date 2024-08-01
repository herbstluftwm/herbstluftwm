#include "widget.h"

#include <algorithm>
#include <functional>

#include "boxstyle.h"
#include "fontdata.h"
#include "utils.h"

using std::function;
using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::pair;

Widget::Widget()
{

}

void Widget::computeGeometry(Rectangle outerGeometry)
{

    geometryCached_ = outerGeometry;
    geometryCached_.width = std::max(geometryCached_.width, minimumSizeCached_.x);
    geometryCached_.height = std::max(geometryCached_.height, minimumSizeCached_.y);
    Rectangle innerGeo = contentGeometryCached();
    int expandingChildrenCount = 0;
    // the dimension/direction in which the children will be stacked
    int Point2D::*stackingDimension = &Point2D::x;
    // the dimension on which all nested widgets agree:
    int Point2D::*fixedDimension = &Point2D::y;
    bool Widget::*expanding = &Widget::expandX_;
    if (vertical_) {
        stackingDimension = &Point2D::y;
        fixedDimension = &Point2D::x;
        expanding = &Widget::expandY_;
    }
    // the remaining space that is not yet occupied by the minimum size
    int stretchSpace = innerGeo.dimensions().*stackingDimension;
    for (const auto& child : nestedWidgets_) {
        if (child->isDisplayNone()) {
            continue;
        }
        stretchSpace -= child->minimumSizeCached().*stackingDimension;
        if (child->*expanding) {
            expandingChildrenCount++;
        }
    }
    // each of the expanding children gets this much extra size on top of their minimum size:
    int stretchSpaceStep = expandingChildrenCount ? (stretchSpace / expandingChildrenCount) : 0;
    Point2D currentTopLeft = innerGeo.tl();
    for (Widget* child : nestedWidgets_) {
        if (child->isDisplayNone()) {
            continue;
        }
        Point2D size = child->minimumSizeCached();
        // in the non-stacking direction, all widgets have the same size
        size.*fixedDimension = innerGeo.dimensions().*fixedDimension;
        // in the stacking direction, let expanding widgets grow according
        // to the remaining space:
        if (child->*expanding && expandingChildrenCount) {
            size.*stackingDimension += stretchSpaceStep;
            stretchSpace -= stretchSpaceStep;
            expandingChildrenCount--;
            if (expandingChildrenCount == 0) {
                // if this was the last expanding widget,
                // then give it all the remaining space:
                size.*stackingDimension += stretchSpace;
            }
        }
        child->computeGeometry({currentTopLeft.x, currentTopLeft.y, size.x, size.y});
        // set the top left point for the next child:
        currentTopLeft.*stackingDimension += size.*stackingDimension;
    }
}

void Widget::computeMinimumSize()
{
    const BoxStyle& style = style_ ? *style_ : BoxStyle::empty();
    Point2D nestedSize = {0, 0};
    // the dimension/direction in which the children will be stacked
    int Point2D::*stackingDimension = &Point2D::x;
    // the dimension on which all nested widgets agree:
    int Point2D::*fixedDimension = &Point2D::y;
    if (vertical_) {
        stackingDimension = &Point2D::y;
        fixedDimension = &Point2D::x;
    }
    for (Widget* child : nestedWidgets_) {
        if (child->isDisplayNone()) {
            continue;
        }
        child->computeMinimumSize();
        auto childSize = child->minimumSizeCached();
        nestedSize.*stackingDimension += childSize.*stackingDimension;
        nestedSize.*fixedDimension = std::max(nestedSize.*fixedDimension,
                                              childSize.*fixedDimension) ;
    }
    Point2D surroundingsSize = {0, 0};
    surroundingsSize.x += style.marginLeft + style.marginRight;
    surroundingsSize.y += style.marginTop + style.marginBottom;
    surroundingsSize.x += style.borderWidthLeft + style.borderWidthRight;
    surroundingsSize.y += style.borderWidthTop + style.borderWidthBottom;
    surroundingsSize.x += style.paddingLeft + style.paddingRight;
    surroundingsSize.y += style.paddingTop + style.paddingBottom;

    Point2D textSize = {0, 0};
    if (textContent_) {
        HSFont font = style.getFont();
        textSize.y =
                style.textHeight.rightOr(font.data().ascent)
                + style.textDepth.rightOr(font.data().descent);
    }
    minimumSizeCached_ = surroundingsSize +
            Point2D::fold(
                [](int a, int b) { return std::max(a,b); },
    {minimumSizeUser_, nestedSize, textSize, {style.minWidth, style.minHeight}});
}

Rectangle Widget::contentGeometryCached() const
{
    if (style_) {
        return geometryCached_.adjusted(
                -style_->marginLeft - style_->borderWidthLeft - style_->paddingLeft,
                -style_->marginTop - style_->borderWidthTop - style_->paddingTop,
                -style_->marginRight - style_->borderWidthRight - style_->paddingRight,
                -style_->marginBottom - style_->borderWidthBottom - style_->paddingBottom);
    }
    return geometryCached_;
}

void Widget::moveGeometryCached(Point2D delta)
{
    geometryCached_.x += delta.x;
    geometryCached_.y += delta.y;
    for (Widget* child : nestedWidgets_) {
        child->moveGeometryCached(delta);
    }
}

void Widget::clearChildren()
{
    for (const auto& child : nestedWidgets_) {
        child->parent_ = nullptr;
        child->indexInParent_ = 0;
    }
    nestedWidgets_.clear();
}

void Widget::addChild(Widget* child)
{
    child->parent_ = this;
    child->indexInParent_ = nestedWidgets_.size();
    nestedWidgets_.push_back(child);
}

void Widget::removeChild(size_t idx)
{
    if (idx >= nestedWidgets_.size()) {
        return;
    }
    nestedWidgets_[idx]->parent_ = nullptr;
    nestedWidgets_[idx]->indexInParent_ = 0;
    auto idxTyped = static_cast<vector<Widget*>::difference_type>(idx);
    nestedWidgets_.erase(nestedWidgets_.begin() + idxTyped);
    // update the index in later children
    for (vector<Widget*>::iterator it = nestedWidgets_.begin() + idxTyped;
         it != nestedWidgets_.end(); it++) {
        (*it)->indexInParent_ --;
    }
}

void Widget::setStyle(shared_ptr<BoxStyle> style)
{
    style_ = style;
}

bool Widget::isDisplayNone() const
{
    return style_ && style_->display == CssDisplay::none;
}

void Widget::recurse(function<void (Widget&)> body)
{
    body(*this);
    for (Widget* child : nestedWidgets_) {
        child->recurse(body);
    }
}

const DomTree* Widget::parent() const
{
    return parent_;
}

const DomTree* Widget::nthChild(size_t idx) const
{
    if (idx < nestedWidgets_.size()) {
        return nestedWidgets_[idx];
    }
    return nullptr;
}

const DomTree* Widget::leftSibling() const
{
    if (parent_ && indexInParent_ > 0) {
        return parent_->nthChild(indexInParent_ - 1);
    }
    return nullptr;
}

bool Widget::hasClass(const CssName& className) const
{
    return classes_.contains(className);
}

size_t Widget::childCount() const
{
    return nestedWidgets_.size();
}

void Widget::setClasses(const CssNameSet& classes)
{
    classes_ = classes;
}

void Widget::setClassEnabled(const CssName& className, bool enabled)
{
    classes_.setEnabled(className, enabled);
}

class WidgetTI : public TreeInterface {
public:
    WidgetTI(Widget* wid) : widget_(wid) {}
    virtual shared_ptr<TreeInterface> nthChild(size_t idx) override {
        return make_shared<WidgetTI>(widget_->nestedWidgets_[idx]);
    }
    virtual size_t childCount() override {
        return widget_->nestedWidgets_.size();
    }
    virtual void appendCaption(Output output) override {
        output << " ";
        for (const auto& name : widget_->classes_.toVector()) {
            output << "." << name.str();
        }
    }
private:
    Widget* widget_;
};

shared_ptr<TreeInterface> Widget::treeInterface()
{
    return make_shared<WidgetTI>(this);
}

