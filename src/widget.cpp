#include "widget.h"

#include <algorithm>
#include <functional>

#include "css.h"
#include "globals.h"

using std::function;

Widget::Widget()
{

}

void Widget::computeGeometry(Rectangle outerGeometry)
{

    geometryCached_ = outerGeometry;
    geometryCached_.width = std::max(geometryCached_.width, minimumSizeCached_.x);
    geometryCached_.height = std::max(geometryCached_.height, minimumSizeCached_.y);
    const ComputedStyle& style = style_ ? *style_ : ComputedStyle::empty;
    Rectangle innerGeo = geometryCached_.adjusted(
                -style.borderWidthLeft - style.paddingLeft,
                -style.borderWidthTop - style.paddingTop,
                -style.borderWidthRight - style.paddingRight,
                -style.borderWidthBottom - style.paddingBottom);
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
        stretchSpace -= child->minimumSizeCached().*stackingDimension;
        if (child->*expanding) {
            expandingChildrenCount++;
        }
    }
    // each of the expanding children gets this much extra size on top of their minimum size:
    int stretchSpaceStep = expandingChildrenCount ? (stretchSpace / expandingChildrenCount) : 0;
    Point2D currentTopLeft = innerGeo.tl();
    for (Widget* child : nestedWidgets_) {
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
    Point2D nestedSize = {0, 0};
    // the dimension/direction in which the children will be stacked
    int Point2D::*stackingDimension = &Point2D::x;
    // the dimension on which all nested widgets agree:
    int Point2D::*fixedDimension = &Point2D::y;
    if (vertical_) {
        stackingDimension = &Point2D::y;
        fixedDimension = &Point2D::x;
    } else {
    }
    for (Widget* child : nestedWidgets_) {
        child->computeMinimumSize();
        auto childSize = child->minimumSizeCached();
        nestedSize.*stackingDimension += childSize.*stackingDimension;
        nestedSize.*fixedDimension = std::max(nestedSize.*fixedDimension,
                                              childSize.*fixedDimension) ;
    }
    const ComputedStyle& style = style_ ? *style_ : ComputedStyle::empty;
    Point2D surroundingsSize = {0, 0};
    surroundingsSize.x += style.borderWidthLeft + style.borderWidthRight;
    surroundingsSize.y += style.borderWidthTop + style.borderWidthBottom;
    surroundingsSize.x += style.paddingLeft + style.paddingRight;
    surroundingsSize.y += style.paddingTop + style.paddingBottom;

    minimumSizeCached_ =
            Point2D::fold(
                [](int a, int b) { return std::min(a,b); },
    {minimumSizeUser_, nestedSize + surroundingsSize});
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
