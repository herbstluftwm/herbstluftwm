/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_MONITOR_MANAGER_H_
#define __HERBSTLUFT_MONITOR_MANAGER_H_

#include "monitor.h"
#include "childbyindex.h"
#include "byname.h"
#include "child.h"

class TagManager;

class MonitorManager : public ChildByIndex<HSMonitor> {
public:
    MonitorManager(TagManager* tags);
    Child_<HSMonitor> focus;

    void clearChildren();
    void ensure_monitors_are_available();
private:
    ByName by_name;
    TagManager* tags;
};


#endif
