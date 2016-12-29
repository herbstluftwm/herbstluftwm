#ifndef __CLIENT_OBJECT_H_
#define __CLIENT_OBJECT_H_

#include <string>

#include "object.h"
#include "attribute_.h"
#include "client.h"

class ClientObject : public Object, public HSClient {
public:
    ClientObject(Window w, bool already_visible);
    virtual ~ClientObject();
    Attribute_<std::string> window_id_str;
};



#endif
