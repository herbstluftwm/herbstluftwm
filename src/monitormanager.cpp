
#include <memory>
#include "monitormanager.h"

using namespace std;

MonitorManager::MonitorManager() {
    by_name = make_shared<Object>();
    addChild(by_name, "by-name");
}
