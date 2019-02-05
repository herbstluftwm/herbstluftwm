#include "frametree.h"

#include "client.h"
#include "layout.h"

FrameTree::FrameTree(HSTag* tag, Settings* settings)
    : tag_(tag)
    , settings_(settings)
{
    root_ = std::make_shared<HSFrameLeaf>(tag, settings, std::shared_ptr<HSFrameSplit>());
    (void) tag_;
    (void) settings_;
}

void FrameTree::foreachClient(std::function<void(Client*)> action)
{
    root_->foreachClient(action);
}

void FrameTree::dump(std::shared_ptr<HSFrame> frame, Output output)
{

    auto onLeaf = [&output](std::shared_ptr<HSFrameLeaf> l) {
        output << LAYOUT_DUMP_BRACKETS[0]
               << "clients"
               << LAYOUT_DUMP_WHITESPACES[0]
               << g_layout_names[l->layout] << ":"
               << l->selection;
        for (auto client : l->clients) {
            output << LAYOUT_DUMP_WHITESPACES[0]
                   << "0x"
                   << std::hex << client->x11Window() << std::dec;
        }
        output << LAYOUT_DUMP_BRACKETS[1];
    };
    auto onSplit = [&output](std::shared_ptr<HSFrameSplit> s) {
        output
            << LAYOUT_DUMP_BRACKETS[0]
            << "split"
            << LAYOUT_DUMP_WHITESPACES[0]
            << g_align_names[s->align_]
            << LAYOUT_DUMP_SEPARATOR
            << ((double)s->fraction_) / (double)FRACTION_UNIT
            << LAYOUT_DUMP_SEPARATOR
            << s->selection_
            << LAYOUT_DUMP_WHITESPACES[0];
        FrameTree::dump(s->a_, output);
        output << LAYOUT_DUMP_WHITESPACES[0];
        FrameTree::dump(s->b_, output);
        output << LAYOUT_DUMP_BRACKETS[1];
    };
    frame->switchcase(onSplit, onLeaf);
}

