#ifndef WIDGET_H
#define WIDGET_H

#include "rectangle.h"

#include <vector>
#include <memory>

class ComputedStyle;

class Widget
{
public:
    Widget();
    /** whether the children of this widged are placed below each other */
    bool vertical_ = true;
    /** whether this widget likes growing into the X-direction */
    bool expandX_ = false;
    /** whether this widget likes growing into the Y-direction */
    bool expandY_ = false;
    std::vector<Widget> nestedWidgets_;
    std::shared_ptr<const ComputedStyle> style_;
    void computeGeometry(Rectangle outerGeometry);
    void computeMinimumSize();
    Point2D minimumSizeCached() const {
        return minimumSizeCached_;
    }
    Rectangle geometryCached() const {
        return geometryCached_;
    }

    Point2D minimumSizeUser_ = {0, 0}; //! custom minimum size
private:
    Rectangle geometryCached_;
    Point2D minimumSizeCached_;
};

#endif // WIDGET_H
