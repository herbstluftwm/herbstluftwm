#include "tilingresult.h"

using std::make_pair;

TilingStep::TilingStep(Rectangle rect)
    : geometry(rect)
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
