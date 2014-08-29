#include "thread.h"
#include "led-matrix.h"

#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

using std::min;
using std::max;

// Base-class for a Thread that does something with a matrix.
class RGBMatrixManipulator : public Thread {
public:
  RGBMatrixManipulator(RGBMatrix *m) : running_(true), matrix_(m) {}
  virtual ~RGBMatrixManipulator() { running_ = false; }

  // Run() implementation needs to check running_ regularly.

protected:
  volatile bool running_;  // TODO: use mutex, but this is good enough for now.
  RGBMatrix *const matrix_;
};

// Pump pixels to screen. Needs to be high priority real-time because jitter
// here will make the PWM uneven.
class DisplayUpdater : public RGBMatrixManipulator {
public:
  DisplayUpdater(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  void Run() {
    while (running_) {
      matrix_->UpdateScreen();
    }
  }
};

int main(int argc, char *argv[]) {
  GPIO io;
  if (!io.Init())
    return 1;

  //Init display
  RGBMatrix m(&io);
    
  RGBMatrixManipulator *image_gen = NULL;

  if (image_gen == NULL)
    return 1;

  //Start the UpdateScreen thread
  RGBMatrixManipulator *updater = new DisplayUpdater(&m);
  updater->Start(10);  // high priority

  delete updater;

  m.ClearScreen();
  m.UpdateScreen();

  return 0;
}
