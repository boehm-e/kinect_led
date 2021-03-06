/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */



#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
/*#include "version.h"*/

#include "ws2811.h"


#define ARRAY_SIZE(stuff)                        (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ                              WS2811_TARGET_FREQ
#define GPIO_PIN                                 12
#define DMA                                      5
#define STRIP_TYPE				                 WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE				             WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE				             SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define WIDTH                                    144
#define HEIGHT                                   1
#define LED_COUNT                                (WIDTH * HEIGHT)







int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;

int clear_on_exit = 0;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};


static uint8_t running = 1;




/* KINECT */

#include <assert.h>
#include "libfreenect.h"

#include <pthread.h>

#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <math.h>

pthread_t freenect_thread;
volatile int diee = 0;

int g_argc;
char **g_argv;

int window;
ws2811_led_t *matrix;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

// back: owned by libfreenect (implicit for depth)
// mid: owned by callbacks, "latest frame ready"
// front: owned by GL, "currently being drawn"
uint8_t *depth_mid, *depth_front;

GLuint gl_depth_tex;

freenect_context *f_ctx;
freenect_device *f_dev;
int freenect_angle = 0;
int freenect_led;


pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_depth = 0;


void chunk_cb(void *buffer, void *pkt_data, int pkt_num, int pkt_size,void *ud)
{
  if(pkt_num == 73 || pkt_num == 146) return;
  uint8_t *raw = (uint8_t *) pkt_data;
  uint16_t *frame=(uint16_t *)buffer;
  if(pkt_num > 219){
    raw += (pkt_num-220) * 12;
    frame += 320 * (pkt_num-2);
  }else if(pkt_num > 146){
    raw += (pkt_num-147) * 12 + 4;
    frame += 320 * (pkt_num-2);
  }else if(pkt_num > 73){
    raw += (pkt_num-74) * 12 + 8;
    frame += 320 * (pkt_num-1);
  }else{
    raw += pkt_num * 12;
    frame += 320 * pkt_num;
  }

  int n = 0;
  while(n != 40){
    frame[0] =  (raw[0]<<3)  | (raw[1]>>5);
    frame[1] = ((raw[2]<<9)  | (raw[3]<<1) | (raw[4]>>7) ) & 2047;
    frame[2] = ((raw[5]<<7)  | (raw[6]>>1) )           & 2047;
    frame[3] = ((raw[8]<<5)  | (raw[9]>>3) )           & 2047;
    void chunk_cb(void *buffer, void *pkt_data, int pkt_num, int pkt_size,void *ud)
    {
      if(pkt_num == 73 || pkt_num == 146) return;
      uint8_t *raw = (uint8_t *) pkt_data;
      uint16_t *frame=(uint16_t *)buffer;
      if(pkt_num > 219){
	raw += (pkt_num-220) * 12;
	frame += 320 * (pkt_num-2);
      }else if(pkt_num > 146){
	raw += (pkt_num-147) * 12 + 4;
	frame += 320 * (pkt_num-2);
      }else if(pkt_num > 73){
	raw += (pkt_num-74) * 12 + 8;
	frame += 320 * (pkt_num-1);
      }else{
	raw += pkt_num * 12;
	frame += 320 * pkt_num;
      }

      int n = 0;
      while(n != 40){
	frame[0] =  (raw[0]<<3)  | (raw[1]>>5);
	frame[1] = ((raw[2]<<9)  | (raw[3]<<1) | (raw[4]>>7) ) & 2047;
	frame[2] = ((raw[5]<<7)  | (raw[6]>>1) )           & 2047;
	frame[3] = ((raw[8]<<5)  | (raw[9]>>3) )           & 2047;
	frame[4] =  (raw[11]<<3)  | (raw[12]>>5);
	frame[5] = ((raw[13]<<9)  | (raw[14]<<1) | (raw[15]>>7) ) & 2047;
	frame[6] = ((raw[16]<<7)  | (raw[17]>>1) )           & 2047;
	frame[7] = ((raw[19]<<5)  | (raw[20]>>3) )           & 2047;
	frame+=8;
	raw+=22;
	n++;
      }

    }
    frame[4] =  (raw[11]<<3)  | (raw[12]>>5);
    frame[5] = ((raw[13]<<9)  | (raw[14]<<1) | (raw[15]>>7) ) & 2047;
    frame[6] = ((raw[16]<<7)  | (raw[17]>>1) )           & 2047;
    frame[7] = ((raw[19]<<5)  | (raw[20]>>3) )           & 2047;
    frame+=8;
    raw+=22;
    n++;
  }

}


void DrawGLScene()
{
  pthread_mutex_lock(&gl_backbuf_mutex);

  while (!got_depth) {
    pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
  }


  uint8_t *tmp;

  if (got_depth) {
    tmp = depth_front;
    depth_front = depth_mid;
    depth_mid = tmp;
    got_depth = 0;
  }

  pthread_mutex_unlock(&gl_backbuf_mutex);

  glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, 320, 240, 0, GL_RGB, GL_UNSIGNED_BYTE, depth_front);

  glBegin(GL_TRIANGLE_FAN);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glTexCoord2f(0, 0); glVertex3f(0,0,0);
  glTexCoord2f(1, 0); glVertex3f(320,0,0);
  glTexCoord2f(1, 1); glVertex3f(320,240,0);
  glTexCoord2f(0, 1); glVertex3f(0,240,0);
  glEnd();

  glutSwapBuffers();
}


void keyPressed(unsigned char key, int x, int y)
{
  if (key == 27) {
    diee = 1;
    pthread_join(freenect_thread, NULL);
    glutDestroyWindow(window);
    free(depth_mid);
    free(depth_front);
    // Not pthread_exit because OSX leaves a thread lying around and doesn't exit
    exit(0);
  }
  if (key == 'w') {
    freenect_angle++;
    if (freenect_angle > 30) {
      freenect_angle = 30;
    }
  }
  if (key == 's') {
    freenect_angle = 0;
  }
  if (key == 'x') {
    freenect_angle--;
    if (freenect_angle < -30) {
      freenect_angle = -30;
    }
  }
  if (key == '1') {
    freenect_set_led(f_dev,LED_GREEN);
  }
  if (key == '2') {
    freenect_set_led(f_dev,LED_RED);
  }
  if (key == '3') {
    freenect_set_led(f_dev,LED_YELLOW);
  }
  if (key == '4') {
    freenect_set_led(f_dev,LED_BLINK_GREEN);
  }
  if (key == '5') {
    // 5 is the same as 4
    freenect_set_led(f_dev,LED_BLINK_GREEN);
  }
  if (key == '6') {
    freenect_set_led(f_dev,LED_BLINK_RED_YELLOW);
  }
  if (key == '0') {
    freenect_set_led(f_dev,LED_OFF);
  }
  freenect_set_tilt_degs(f_dev,freenect_angle);
}


void ReSizeGLScene(int Width, int Height)
{
  glViewport(0,0,Width,Height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, 320, 240, 0, -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void InitGL(int Width, int Height)
{
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_TEXTURE_2D);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glShadeModel(GL_FLAT);

  glGenTextures(1, &gl_depth_tex);
  glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  ReSizeGLScene(Width, Height);
}

void *gl_threadfunc(void *arg)
{
  printf("GL thread\n");

  glutInit(&g_argc, g_argv);

  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);
  glutInitWindowSize(320, 240);
  glutInitWindowPosition(0, 0);

  window = glutCreateWindow("LibFreenect");

  glutDisplayFunc(&DrawGLScene);
  glutIdleFunc(&DrawGLScene);
  glutReshapeFunc(&ReSizeGLScene);
  glutKeyboardFunc(&keyPressed);

  InitGL(320, 240);

  glutMainLoop();

  return NULL;
}

uint16_t t_gamma[2048];

int dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
ws2811_led_t dotcolors[] =
{
    0x00200000,  // red
    0x00201000,  // orange
    0x00202000,  // yellow
    0x00200010,  // pink
    0x00002020,  // lightblue
    0x00000020,  // blue
    0x00100010,  // purple
    0x00200010,  // pink
};

ws2811_led_t dotcolors_rgbw[] =
{
    0x00200000,  // red
    0x10200000,  // red + W
    0x00200010,  // pink
    0x00200010,  // pink
    0x00000020,  // blue
    0x10000020,  // blue + W
    0x00101010,  // white
    0x10101010,  // white + W

};

void matrix_render(void)
{
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = matrix[y * width + x];
        }
    }
}


void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
  int i;
  uint16_t *depth = (uint16_t*)v_depth;
  pthread_mutex_lock(&gl_backbuf_mutex);


  ws2811_return_t ret;



  
  int x;

  int j = 0;
  // MY CODE

  int begin = 320*(240/2);
  int end = begin + 240;

  for (i=begin; i<end; i++) {
    int pval = t_gamma[depth[i]];
    int lb = pval & 0xff;

    int mapLed = (int) ((i-begin) * 144 / 240);
    
    if (pval>>8 > 15) {
      matrix[mapLed] = 0xffffffff;
      ledstring.channel[0].leds[mapLed] = matrix[mapLed];
    } else {
      matrix[mapLed] = 0x00000000;
      ledstring.channel[0].leds[mapLed] = matrix[mapLed];
    }


    if (i%15 == 0) { // DISPLAY ONLY EVERY 20
      printf("STRIPE : [$d] : %d : %d\n", mapLed, lb, pval>>8);
    }
    j++;
  }
    ws2811_render(&ledstring);
    matrix_render();


  for (i=0; i<320*240; i++) {
    int pval = t_gamma[depth[i]];
    int lb = pval & 0xff;
    switch (pval>>8) {
    case 0:
      depth_mid[3*i+0] = 255;
      depth_mid[3*i+1] = 255-lb;
      depth_mid[3*i+2] = 255-lb;
      break;
    case 1:
      depth_mid[3*i+0] = 255;
      depth_mid[3*i+1] = lb;
      depth_mid[3*i+2] = 0;
      break;
    case 2:
      depth_mid[3*i+0] = 255-lb;
      depth_mid[3*i+1] = 255;
      depth_mid[3*i+2] = 0;
      break;
    case 3:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 255;
      depth_mid[3*i+2] = lb;
      break;
    case 4:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 255-lb;
      depth_mid[3*i+2] = 255;
      break;
    case 5:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 0;
      depth_mid[3*i+2] = 255-lb;
      break;
    default:
      depth_mid[3*i+0] = 0;
      depth_mid[3*i+1] = 0;
      depth_mid[3*i+2] = 0;
      break;
    }
  }
  got_depth++;
  pthread_cond_signal(&gl_frame_cond);
  pthread_mutex_unlock(&gl_backbuf_mutex);
}



void matrix_bottom(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(dotspos); i++)
    {
      dotspos[i]++;
      if (dotspos[i] > (width - 1))
        {
	  dotspos[i] = 0;
        }

      if (ledstring.channel[0].strip_type == SK6812_STRIP_RGBW) {
	matrix[dotspos[i] + (height - 1) * width] = dotcolors_rgbw[i];
      } else {
	matrix[dotspos[i] + (height - 1) * width] = dotcolors[i];
      }
    }
}

static void ctrl_c_handler(int signum)
{
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void *freenect_threadfunc(void *arg)
{
  int accelCount = 0;
  ws2811_return_t ret;

  //freenect_set_tilt_degs(f_dev,freenect_angle);
  //freenect_set_led(f_dev,LED_RED);
  freenect_set_depth_callback(f_dev, depth_cb);
  freenect_set_depth_mode(f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT_PACKED));
  freenect_set_depth_chunk_callback(f_dev,chunk_cb);
  freenect_start_depth(f_dev);

  printf("'w'-tilt up, 's'-level, 'x'-tilt down, '0'-'6'-select LED mode\n");



  //KINECT && STRIPE

  matrix = malloc(sizeof(ws2811_led_t) * width * height);

  setup_handlers();

  if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
  }

  
  while (!diee && freenect_process_events(f_ctx) >= 0) {
    //    printf("DBG\n");
    
    /*int x;

      for (x = 0; x < width; x++)
      {
      printf("HEY : %d\n", x);
      matrix[x] = 0x00202f00;
      //	matrix[x] = 0x000000ff;
      ledstring.channel[0].leds[x] = matrix[x];
      //	usleep(1000000 / 15);
      }
      ws2811_render(&ledstring);
      matrix_render();
    */
  }

  printf("\nshutting down streams...\n");

  freenect_stop_depth(f_dev);

  freenect_close_device(f_dev);
  freenect_shutdown(f_ctx);

  printf("-- done!\n");
  return NULL;
}


/*END KINECT*/

void matrix_clear(void)
{
    int x, y;

    for (y = 0; y < (height ); y++)
    {
        for (x = 0; x < width; x++)
        {
            matrix[y * width + x] = 0;
        }
    }
}

// MAIN KINECT

int main(int argc, char **argv)
{
  int res;

  depth_mid = (uint8_t*)malloc(320*240*3);
  depth_front = (uint8_t*)malloc(320*240*3);

  printf("Kinect camera test\n");

  int i;
  for (i=0; i<2048; i++) {
    float v = i/2048.0;
    v = powf(v, 3)* 6;
    t_gamma[i] = v*6*256;
  }

  g_argc = argc;
  g_argv = argv;

  if (freenect_init(&f_ctx, NULL) < 0) {
    printf("freenect_init() failed\n");
    return 1;
  }

  freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
  freenect_select_subdevices(f_ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

  int nr_devices = freenect_num_devices (f_ctx);
  printf ("Number of devices found: %d\n", nr_devices);

  int user_device_number = 0;
  if (argc > 1)
    user_device_number = atoi(argv[1]);

  if(nr_devices < 1) {
    freenect_shutdown(f_ctx);
    return 1;
  }

  if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
    printf("Could not open device\n");
    freenect_shutdown(f_ctx);
    return 1;
  }

  res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
  if (res) {
    printf("pthread_create failed\n");
    freenect_shutdown(f_ctx);
    return 1;
  }

  // OS X requires GLUT to run on the main thread
  gl_threadfunc(NULL);

  return 0;
}
