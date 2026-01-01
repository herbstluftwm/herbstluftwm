#ifndef LAYOUTALGOIMPL_H
#define LAYOUTALGOIMPL_H

#include <memory>

#include "framedata.h"
#include "tilingresult.h"

class Settings;

/**
 * @brief An interface for the implementation of
 * layout algorithms.
 */
class LayoutAlgoImpl {
public:
    virtual TilingResult compute(Rectangle rect) = 0;
    virtual int neighbour(Direction direction, DirectionLevel depth, int startIndex) = 0;

    //! create a new algorithm instance tied to a given frame
    static std::unique_ptr<LayoutAlgoImpl> createInstance(const FrameLeaf& frame, LayoutAlgorithm algoName);

    virtual ~LayoutAlgoImpl();
    LayoutAlgorithm name() { return algoName_; }
protected:
    /** the parameters that need to be passed down to
     *  the constructor of LayoutAlgoImpl
     */
    class Params {
    public:
        const FrameLeaf& frame;
        LayoutAlgorithm algoName;
    };
    const FrameLeaf& frame_;
    Settings* settings_;
    LayoutAlgoImpl(Params p);
private:
    LayoutAlgorithm algoName_;
};

#endif // LAYOUTALGOIMPL_H
