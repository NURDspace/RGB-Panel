#include "thread.h"
#include "led-matrix.h"

#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <algorithm>

#define PI 3.14159265

using std::min;
using std::max;


int make_socket (uint16_t port)
     {
       int sock;
       struct sockaddr_in name;
       /* Create the socket. */
       sock = socket (PF_INET, SOCK_STREAM, 0);
       if (sock < 0)
         {
           perror ("socket");
           exit (EXIT_FAILURE);
         }
       /* Give the socket a name. */
       name.sin_family = AF_INET;
       name.sin_port = htons (port);
       name.sin_addr.s_addr = htonl (INADDR_ANY);
       if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
         {
           perror ("bind");
           exit (EXIT_FAILURE);
         }
       return sock;
     }

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

// Pump pixels to screen. Needs to be high priority real-time because jitter
// here will make the PWM uneven.
class EthernetListener : public RGBMatrixManipulator {
public:
  EthernetListener(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };
  void Run() {
    int sockfd, newsockfd;
    uint8_t buffer[matrix_->frame_size()*matrix_->pwm_bits()];
    struct sockaddr_in cli_addr;
    unsigned long n, framepos;
    int pwm;

    sockfd = make_socket(5001);

    listen(sockfd,5);
    socklen_t clilen = sizeof(cli_addr);
    while (running_) {

      /* Accept actual connection from the client */
      newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, 
                                  &clilen);
      printf("Recieving...\n");
      framepos=0;
      pwm=0;
      if (newsockfd < 0) 
      {
          perror("ERROR on accept");
      }
      /* If connection is established then start communicating */
      memset(buffer,0,sizeof(buffer));

      uint8_t frame [matrix_->frame_size()];
      memset(frame,0,sizeof(frame));
      while (true) {
        n = read( newsockfd,buffer,sizeof(buffer));
        if (n == 0) {break;}
        for (unsigned long x = 0; x < n; ++x) {
          uint8_t b = buffer[x];
          if (b & 128) { //new frame
            matrix_->FlipBuffer();
//            printf("Flip!\n",pwm);
            memset(frame,0,sizeof(frame));
            pwm = 0;
          }
          if (b & 64) { //new pwm frame
            if (pwm<=matrix_->pwm_bits()) {
              matrix_->PushBuffer(frame,pwm);
            }
//            printf("pwm is now %d.\n",pwm);
            pwm++;
//            printf("frame: ");
//            for (long i=0;i < sizeof(frame);i++) {
//              printf("%x",frame[i]);
//            }
//            printf("\n");
            framepos=0;
          } else {
            framepos++;
          }
          if (framepos<= sizeof(frame)) {
            frame[framepos]=b;
          } //otherwise discard
        }
      }
      matrix_->FlipBuffer();
    }
  }
};

// -- The following are demo image generators.

// Simple generator that pulses through RGB and White.
class ColorPulseGenerator : public RGBMatrixManipulator {
public:
  ColorPulseGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int width = matrix_->width();
    const int height = matrix_->height();
    uint32_t count = 0;
    while (running_) {
//      usleep(8000);
      ++count;
      int color = (count >> 9) % 6;
      int value = count & 0xFF;
      if (count & 0x100) value = 255 - value;
      int r, g, b;
      switch (color) {
      case 0: r = value; g = b = 0; break;
      case 1: r = g = value; b = 0; break;
      case 2: g = value; r = b = 0; break;
      case 3: g = b = value; r = 0; break;
      case 4: b = value; r = g = 0; break;
      default: r = g = b = value; break;
      }
      for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
          matrix_->SetPixel(x, y, r, g, b);
    }
    matrix_->FlipBuffer();
  }
};

// Simple generator that waves through all colours.
class ColorWaveGenerator : public RGBMatrixManipulator {
public:
  ColorWaveGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int width = matrix_->width();
    const int height = matrix_->height();
    uint32_t count = 0;
    while (running_) {
//      usleep(500+(count%100)*5);
      ++count;
      int r, g, b;
//      if (count & 0x1000000) value = 255 - value;
      for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
          r = int((sin(( count*1.0+x*3)*PI/180)+1)*128) & 0xFF;
          g = int((sin(( count*3.5+x*5)*PI/180)+1)*128) & 0xFF;
          b = int((sin((-count*0.9+x*7)*PI/180)+1)*128) & 0xFF;
          matrix_->SetPixel(x, y, r, g, b);
        }
      }
      matrix_->FlipBuffer();
    }
  }
};

//Testing
class TestWaveGenerator : public RGBMatrixManipulator {
public:
  TestWaveGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int width = matrix_->width();
    const int height = matrix_->height();
    uint32_t count = 0;
    while (running_) {
//      usleep(500+(count%100)*5);
      ++count;
      int r, g, b;
      int color = (count >> 9) % 6;
//      if (count & 0x1000000) value = 255 - value;
      for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
          int value = int((sin(( count*0.1+x*3)*PI/180)+1)*128) & 0xFF;
          switch (color) {
            case 0: r = value; g = b = 0; break;
            case 1: r = g = value; b = 0; break;
            case 2: g = value; r = b = 0; break;
            case 3: g = b = value; r = 0; break;
            case 4: b = value; r = g = 0; break;
            default: r = g = b = value; break;
          }
          matrix_->SetPixel(x, y, r, g, b);
        }
      }
      matrix_->FlipBuffer();
    }
  }
};

class SimpleSquare : public RGBMatrixManipulator {
public:
  SimpleSquare(RGBMatrix *m) : RGBMatrixManipulator(m) {}
  void Run() {
    const int width = matrix_->width();
    const int height = matrix_->height();
    // Diagonaly
    for (int x = 0; x < width; ++x) {
        matrix_->SetPixel(x, x, 255, 255, 255);
        matrix_->SetPixel(height -1 - x, x, 255, 0, 255);
    }
    for (int x = 0; x < width; ++x) {
      matrix_->SetPixel(x, 0, 255, 0, 0);
      matrix_->SetPixel(x, height - 1, 255, 255, 0);
    }
    for (int y = 0; y < height; ++y) {
      matrix_->SetPixel(0, y, 0, 0, 255);
      matrix_->SetPixel(width - 1, y, 0, 255, 0);
    }
    matrix_->FlipBuffer();
  }
};

// Simple class that generates a rotating block on the screen.
class RotatingBlockGenerator : public RGBMatrixManipulator {
public:
  RotatingBlockGenerator(RGBMatrix *m) : RGBMatrixManipulator(m) {}

  uint8_t scale_col(int val, int lo, int hi) {
    if (val < lo) return 0;
    if (val > hi) return 255;
    return 255 * (val - lo) / (hi - lo);
  }

  void Run() {
    const int cent_x = matrix_->width() / 2;
    const int cent_y = matrix_->height() / 2;

    // The square to rotate (inner square + black frame) needs to cover the
    // whole area, even if diagnoal.
    const int rotate_square = min(matrix_->width(), matrix_->height()) * 1.41;
    const int min_rotate = cent_x - rotate_square / 2;
    const int max_rotate = cent_x + rotate_square / 2;

    // The square to display is within the visible area.
    const int display_square = min(matrix_->width(), matrix_->height()) * 0.7;
    const int min_display = cent_x - display_square / 2;
    const int max_display = cent_x + display_square / 2;

    const float deg_to_rad = 2 * 3.14159265 / 360;
    int rotation = 0;
    while (running_) {
      ++rotation;
      usleep(15 * 1000);
      rotation %= 360;
      for (int x = min_rotate; x < max_rotate; ++x) {
        for (int y = min_rotate; y < max_rotate; ++y) {
          float disp_x, disp_y;
          Rotate(x - cent_x, y - cent_y,
                 deg_to_rad * rotation, &disp_x, &disp_y);
          if (x >= min_display && x < max_display &&
              y >= min_display && y < max_display) { // within display square
            matrix_->SetPixel(disp_x + cent_x, disp_y + cent_y,
                              scale_col(x, min_display, max_display),
                              255 - scale_col(y, min_display, max_display),
                              scale_col(y, min_display, max_display));
          } else {
            // black frame.
            matrix_->SetPixel(disp_x + cent_x, disp_y + cent_y, 0, 0, 0);
          }
        }
      }
      matrix_->FlipBuffer();
    }
  }

private:
  void Rotate(int x, int y, float angle,
              float *new_x, float *new_y) {
    *new_x = x * cosf(angle) - y * sinf(angle);
    *new_y = x * sinf(angle) + y * cosf(angle);
  }
};

class ImageScroller : public RGBMatrixManipulator {
public:
  ImageScroller(RGBMatrix *m)
    : RGBMatrixManipulator(m), image_(NULL), horizontal_position_(0) {
  }

  // _very_ simplified. Can only read binary P6 PPM. Expects newlines in headers
  // Not really robust. Use at your own risk :)
  bool LoadPPM(const char *filename) {
    if (image_) {
      delete [] image_;
      image_ = NULL;
    }
    FILE *f = fopen(filename, "r");
    if (f == NULL) return false;
    char header_buf[256];
    const char *line = ReadLine(f, header_buf, sizeof(header_buf));
#define EXIT_WITH_MSG(m) { fprintf(stderr, "%s: %s |%s", filename, m, line); \
      fclose(f); return false; }
    if (sscanf(line, "P6 ") == EOF)
      EXIT_WITH_MSG("Can only handle P6 as PPM type.");
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d %d ", &width_, &height_) != 2)
      EXIT_WITH_MSG("Width/height expected");
    int value;
    line = ReadLine(f, header_buf, sizeof(header_buf));
    if (!line || sscanf(line, "%d ", &value) != 1 || value != 255)
      EXIT_WITH_MSG("Only 255 for maxval allowed.");
    const size_t pixel_count = width_ * height_;
    image_ = new Pixel [ pixel_count ];
    assert(sizeof(Pixel) == 3);   // we make that assumption.
    if (fread(image_, sizeof(Pixel), pixel_count, f) != pixel_count) {
      line = "";
      EXIT_WITH_MSG("Not enough pixels read.");
    }
#undef EXIT_WITH_MSG
    fclose(f);
    fprintf(stderr, "Read image with %dx%d\n", width_, height_);
    horizontal_position_ = 0;
    return true;
  }

  void Run() {
    const int screen_height = matrix_->height();
    const int screen_width = matrix_->width();
    while (running_) {
      if (image_ == NULL) {
        usleep(100 * 1000);
        continue;
      }
      usleep(30 * 1000);
      for (int x = 0; x < screen_width; ++x) {
        for (int y = 0; y < screen_height; ++y) {
          const Pixel &p = getPixel((horizontal_position_ + x) % width_, y);
          // Display upside down on my desk. Lets flip :)
          int disp_x = screen_width -x-1;
          int disp_y = screen_height -y-1;
          matrix_->SetPixel(disp_x, disp_y, p.red, p.green, p.blue);
        }
      }
      ++horizontal_position_;
      matrix_->FlipBuffer();
    }
  }

private:
  struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  // Read line, skip comments.
  char *ReadLine(FILE *f, char *buffer, size_t len) {
    char *result;
    do {
      result = fgets(buffer, len, f);
    } while (result != NULL && result[0] == '#');
    return result;
  }

  const Pixel &getPixel(int x, int y) {
    static Pixel dummy;
    if (x < 0 || x > width_ || y < 0 || y > height_) return dummy;
    return image_[x + width_ * y];
  }

  int width_;
  int height_;
  Pixel *image_;
  uint32_t horizontal_position_;
};

int main(int argc, char *argv[]) {
  int demo = 5;
  if (argc > 1) {
    demo = atoi(argv[1]);
  }
  fprintf(stderr, "Using demo %d\n", demo);

  GPIO io;
  if (!io.Init())
    return 1;

  RGBMatrix m(&io);
    
  RGBMatrixManipulator *image_gen = NULL;
  switch (demo) {
  case 0:
    image_gen = new RotatingBlockGenerator(&m);
    break;

  case 1:
    if (argc > 2) {
      ImageScroller *scroller = new ImageScroller(&m);
      if (!scroller->LoadPPM(argv[2]))
        return 1;
      image_gen = scroller;
    } else {
      fprintf(stderr, "Demo %d Requires PPM image as parameter", demo);
      return 1;
    }
    break;

  case 2:
    image_gen = new SimpleSquare(&m);
    break;

  case 3:
    image_gen = new ColorWaveGenerator(&m);
    break;

  case 4:
    image_gen = new TestWaveGenerator(&m);
    break;

  case 5:
    image_gen = new EthernetListener(&m);
    printf("Hunting on port 5001.\n");
    break;

  default:
    image_gen = new ColorPulseGenerator(&m);
    break;
  }

  if (image_gen == NULL)
    return 1;

//  RGBMatrixManipulator *eth_gen = new EthernetListener(&m);
  RGBMatrixManipulator *updater = new DisplayUpdater(&m);
  updater->Start(10);  // high priority
//  eth_gen->Start();
  image_gen->Start();

  // Things are set up. Just wait for <RETURN> to be pressed.
  printf("Press <RETURN> to exit and reset LEDs\n");
  getchar();

  // Stopping threads and wait for them to join.
  delete image_gen;
  delete updater;

  // Final thing before exit: clear screen and update once, so that
  // we don't have random pixels burn
  m.ClearScreen();
  m.UpdateScreen();

  return 0;
}
