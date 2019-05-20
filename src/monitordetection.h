#pragma once

#include "x11-types.h"

class XConnection;

//! returns a list of monitors or an empty list if no xinerama is present
RectangleVec detectMonitorsXinerama(XConnection& X);

