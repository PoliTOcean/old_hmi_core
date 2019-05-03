#ifndef AUTODRIVE_H
#define AUTODRIVE_H

#include "vision.h"
#include <opencv2/opencv.hpp>
#include "PolitoceanConstants.h"
#include "Publisher.h"

#define MAX_PATH 12

namespace Politocean {

enum class Direction{
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class AutoDrive
{

public:
    AutoDrive();
    AutoDrive(Direction startDirection);
    Direction updateDirection(Mat frame);
    Mat getGrid();
    void reset();


private:
    Point currentPos;
    Direction direction;
    Publisher publisher;
    Direction path[MAX_PATH];
    Mat grid;


};

}

#endif // AUTODRIVE_H