#include "tilingresult.h"
#include "utils.h"

using namespace std;

TilingStep::TilingStep(Rectangle rect)
    : geometry(rect)
    , floated(false)
    , needsRaise(false)
{ }


TilingResult::TilingResult()
    : focus(NULL)
    , focused_frame(NULL)
{ }

TilingStep& TilingResult::operator[](HSClient* client) {
    TilingStep someStep = Rectangle();
    data.push_back(make_pair(client, someStep));
    return data.rbegin()->second;
}

void TilingResult::mergeFrom(TilingResult& other) {
    data.splice(data.end(), other.data);
    frames.splice(frames.end(), other.frames);
}

void TilingResult::add(FrameDecoration* dec, const FrameDecorationData& frame_data) {
    frames.push_back(make_pair(dec,frame_data));
}
