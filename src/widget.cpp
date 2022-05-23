#include "widget.h"

#include <algorithm>
#include <functional>

#include "css.h"

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
                style.borderWidthLeft,
                style.borderWidthTop,
                -style.borderWidthRight,
                -style.borderWidthBottom);
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
    int stretchSpace = innerGeo.y;
    for (const auto& child : nestedWidgets_) {
        if (vertical_) {
            stretchSpace -= child.minimumSizeCached().*stackingDimension;
            if (child.*expanding) {
                expandingChildrenCount++;
            }
        }
    }
    // each of the expanding children gets this much extra size on top of their minimum size:
    int stretchSpaceStep = expandingChildrenCount ? (stretchSpace / expandingChildrenCount) : 0;
    Point2D currentTopLeft = innerGeo.tl();
    for (auto& child : nestedWidgets_) {
        Point2D size = child.minimumSizeCached();
        // in the non-stacking direction, all widgets have the same size
        size.*fixedDimension = innerGeo.dimensions().*fixedDimension;
        // in the stacking direction, let expanding widgets grow according
        // to the remaining space:
        if (child.*expanding && expandingChildrenCount) {
            expandingChildrenCount--;
            stretchSpace -= stretchSpaceStep;
            size.*stackingDimension += stretchSpaceStep;
            if (expandingChildrenCount == 0) {
                // if this was the last expanding widget,
                // then give it all the remaining space:
                size.*stackingDimension += stretchSpace;
            }
        }
        child.computeGeometry({currentTopLeft.x, currentTopLeft.y, size.x, size.y});
        // set the top left point for the next child:
        currentTopLeft.*stackingDimension += size.*stackingDimension;
    }
}

void Widget::computeMinimumSize()
{
    Point2D nestedSize = {0, 0};
    function<void(Point2D)> handleChildSize;
    if (vertical_) {
        handleChildSize = [&nestedSize] (Point2D childSize) {
            nestedSize.x = std::max(nestedSize.x, childSize.x);
            nestedSize.y = nestedSize.y + childSize.x;
        };
    } else {
        handleChildSize = [&nestedSize] (Point2D childSize) {
            nestedSize.x = nestedSize.x + childSize.x;
            nestedSize.y = std::max(nestedSize.y, childSize.x);
        };
    }
    for (auto& child : nestedWidgets_) {
        child.computeMinimumSize();
        handleChildSize(child.minimumSizeCached());
    }
    const ComputedStyle& style = style_ ? *style_ : ComputedStyle::empty;
    Point2D surroundingsSize = {0, 0};
    surroundingsSize.x += style.borderWidthLeft + style.borderWidthRight;
    surroundingsSize.y += style.borderWidthTop + style.borderWidthBottom;
    minimumSizeCached_ =
            Point2D::fold(
                [](int a, int b) { return std::min(a,b); },
                {minimumSizeUser_, nestedSize + surroundingsSize});
}
