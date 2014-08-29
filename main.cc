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

// -- The following are demo image generators.

// Simple generator that pulses through RGB and White.
class EthernetListner: public RGBMatrixManipulator {
public:
  EthernetListner(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
        //Do some network stuff in here
        //Write that stuff to the framebuffer
  }
};

int main(int argc, char *argv[]) {
  GPIO io;
  if (!io.Init())
    return 1;

  RGBMatrix m(&io);

  //Create the ethernet listner thread    
  RGBMatrixManipulator *eth_gen = NULL;
  eth_gen = new EthernetListner(&m);

  //Create the ScreenUpdater thread
  RGBMatrixManipulator *updater = new DisplayUpdater(&m);
  //Start the ScreenUpdater thread
  updater->Start(10);  // high priority

  //Start the ethernet thread
  eth_gen->Start();

  // Things are set up. Just wait for <RETURN> to be pressed.
  printf("Press <RETURN> to exit and reset LEDs\n");
  getchar();

  // Stopping threads and wait for them to join.
  delete eth_gen;
  delete updater;

  // Final thing before exit: clear screen and update once, so that
  // we don't have random pixels burn
  m.ClearScreen();
  m.UpdateScreen();

  return 0;
}
