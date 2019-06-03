#pragma once

class Root;
class XConnection;

class XMainLoop {
public:
    XMainLoop(XConnection& X, Root* root);
    void run();
    //! quit the main loop as soon as possible
    void quit();
private:
    XConnection& X_;
    Root* root_;
    bool aboutToQuit_;
};
