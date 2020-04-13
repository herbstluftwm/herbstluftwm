#include "tilingresult.h"

using std::make_pair;

TilingStep::TilingStep(Rectangle rect)
    : geometry(rect)
{ }

void TilingResult::add(Client* client, const TilingStep& client_data)
{
    data.push_back(make_pair(client, client_data));
}

void TilingResult::add(FrameDecoration* dec, const FrameDecorationData& frame_data) {
    frames.push_back(make_pair(dec,frame_data));
}

void TilingResult::mergeFrom(TilingResult& other) {
    data.splice(data.end(), other.data);
    frames.splice(frames.end(), other.frames);
}
