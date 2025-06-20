#include <iostream>
#include "MediaConverter.h"
#include "MediaStreamer.h"

using namespace std;

int main()
{
//    MediaConverter* cvt = new MediaConverter;
//    delete cvt;

    RTSPServer server(8554);
    server.start();

    return 0;
}
