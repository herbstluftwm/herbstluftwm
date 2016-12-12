/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include "directory.h"
#include "attribute.h"

#include <map>
#include <vector>


#define OBJECT_PATH_SEPARATOR '.'
#define USER_ATTRIBUTE_PREFIX "my_"
#define TMP_OBJECT_PATH "tmp"

namespace herbstluft {

class Object : public Directory {

public:
    Object(const std::string &name = {});
    virtual ~Object() {}

    virtual Type type() { return Type::OBJECT; }

    virtual void print(const std::string &prefix = "\t| "); // a debug method

    // object tree ls command
    virtual void ls(Output out);
    virtual void ls(Path path, Output out); // traversial version

    virtual bool exists(const std::string &name, Type t = Type::DIRECTORY);
    virtual std::string read(const std::string &attr) const;
    virtual bool writeable(const std::string &attr) const;
    virtual void write(const std::string &attr, const std::string &value);
    virtual bool hookable(const std::string &attr) const;

    virtual void trigger(const std::string &action, ArgList args);

    static std::pair<ArgList,std::string> splitPath(const std::string &path);

    // return an attribute if it exists, else NULL
    Attribute* attribute(const std::string &name);

    std::map<std::string, Attribute*> attributes() { return attribs_; }

protected:
    // initialize an attribute (typically used by init())
    virtual void wireAttributes(std::vector<Attribute*> attrs);
    virtual void wireActions(std::vector<Action*> actions);

    std::map<std::string, Attribute*> attribs_;
    std::map<std::string, Action*> actions_;

    //DynamicAttribute nameAttribute_;
};

}

#endif

