#ifndef __CLIENT_OBJECT_H_
#define __CLIENT_OBJECT_H_


#include "object.h"
#include "client.h"

class ClientObject : public Object, public HSClient {
public:
    ClientObject(Window w, bool already_visible);
    virtual ~ClientObject();
};



#endif
