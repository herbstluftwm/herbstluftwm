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

class MonitorManager : public ChildByIndex<HSMonitor> {
public:
    MonitorManager();
    Child_<HSMonitor> focus;
private:
    ByName by_name;
};


#endif
