
#include <memory>
#include "monitormanager.h"

using namespace std;

MonitorManager::MonitorManager()
    : ChildByIndex<HSMonitor>()
    , by_name(*this)
{
}
