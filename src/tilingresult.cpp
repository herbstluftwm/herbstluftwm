#include "tilingresult.h"

using namespace std;

TilingStep::TilingStep(Rectangle rect)
    : geometry(rect)
    , floated(false)
    , needsRaise(false)
{ }


TilingResult::TilingResult()
    : focus(NULL)
{ }

TilingStep& TilingResult::operator[](HSClient* client) {
    TilingStep someStep = Rectangle();
    data.push_back(make_pair(client, someStep));
    return data.rbegin()->second;
}

void TilingResult::mergeFrom(TilingResult& other) {
    data.splice(data.end(), other.data);
}
