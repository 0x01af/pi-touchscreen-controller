/* pi-touchscreen-controller.c - a little program to start slide, dimm and blank the RPi touchscreen
   and stop slide with unblanking touchscreen on touch.
   Original by https://github.com/eskdale/pi-touchscreen-dimmer 

   Loosely based on the original. The main difference is that additionally slide starts first, and definitely blanks the screen at the end.

   On a touch event we reset the brightness to the original value (read when
   the program started.

   Unless you change the permissions in udev this program needs to run as
   root or setuid as root.

   2019-01-22 Dougie Lawson
   (C) Copyright 2019, Dougie Lawson, all right reserved.
   2019-01-24 Jon Eskdale
   2020-01-11 Jon Eskdale change to full brightness if changing when touch detected
   2021-01-01 Olaf Sonderegger add slide
   2021-01-02 Olaf Sonderegger add blanking touchscreen
   2021-01-03 Olaf Sonderegger bugfixing child process handling, add better error handling
   2021-05-01 Olaf Sonderegger add blanking during time period
*/

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* MACRO exitOnError(topic) - Add a topic to stderr message, and exit the program.
 * see also: https://stackoverflow.com/questions/154136/why-use-apparently-meaningless-do-while-and-if-else-statements-in-macros/154138#154138
 */
#define exitOnError(topic) do { perror(topic); \
                                exit(EXIT_FAILURE); \
                              } while (0)

/* function msleep(msec) - Sleep for the requested number of milliseconds.
 * see also: https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
 */
int
msleep(long msec)
{
  struct timespec ts;
  int res;

  if (msec < 0) {
    errno = EINVAL;
    return -1;
  }

  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  do {
    res = nanosleep(&ts, &ts);
  } while (res && errno == EINTR);

  return res;
}

long int
readint(char* filenm)
{
  FILE* filefd;
  filefd = fopen(filenm, "r");
  if(filefd == NULL){
    int err = errno;
    printf("Error opening %s file: %d", filenm, err);
    exit(1);
  }
  
  char number[10];
  char* end;
  fscanf(filefd, "%s", &number);
  printf("File: %s ,The number is: %s\n",filenm, number);
  
  fclose(filefd);

  return strtol(number,&end,10);
}

int
main(int argc, char* argv[])
{
  printf("\n***\npi-touch-controller v2021-05-01\n(c) 2021 Olaf Sonderegger\n***\n\n");

  if (argc < 4) {
    printf("Usage: timeout <slide_timeout_sec> <slide_pictures_path> <dimmer_timeout_sec> <min_brightness> <blank_timeout_sec> <device> [<device>...]\n");
    printf("    slide_timeout: minutes after last touch event\n");
    printf("    dimmer_timeout: minutes after slide has been started (0 = immediately)\n");
    printf("    min_brightness: brightness value of screen (254 = max. value; 50 = allowed min. value)\n");
    printf("    blank_period: period during sceen will be blanked (hh:mm-hh:mm, 24h format; default = 00:00-00:00)\n");
    printf("    Use lsinput to see input devices.\n");
    printf("    Device to use is shown as /dev/input/<device>\n");
    exit(1);
  }

  int i;
  int tlen;
  struct stat sb;
 
  int slide_timeout;
  char slide_pictures[255];
  int dimmer_timeout;
  int min_brightness;
  struct tm tm_blank;
  struct tm tm_unblank;

  tlen = strlen(argv[1]);
  for (i=0;i<tlen; i++)
    if (!isdigit(argv[1][i])) {
      printf ("Entered slide_timeout value is not a number\n");
      exit(1);
    }

  slide_timeout = atoi(argv[1]) * 60;
  printf ("slide_timeout = %d seconds (after last touch event)\n", slide_timeout);

  strcpy(slide_pictures, argv[2]);
  // see also: https://stackoverflow.com/questions/12510874/how-can-i-check-if-a-directory-exists
  if(stat(slide_pictures, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
    printf ("Entered slide_pictures path is not a directory: %s\n", slide_pictures);
    exit(1);
  }
  printf ("slide_pictures = folder %s\n", slide_pictures);

  tlen = strlen(argv[3]);
  for (i=0;i<tlen; i++)
    if (!isdigit(argv[3][i])) {
      printf ("Entered dimmer_timeout value is not a number\n");
      exit(1);
    }

  dimmer_timeout = atoi(argv[3]) * 60;
  printf ("dimmer_timeout = %d seconds (after slide_timeout reached)\n", dimmer_timeout);

  tlen = strlen(argv[4]);
  for (i=0;i<tlen; i++)
    if (!isdigit(argv[4][i])) {
      printf ("Entered Min Brightness value is not a number\n");
      exit(1);
    }

  min_brightness = atoi(argv[4]);
  if (min_brightness > 254 || min_brightness < 50) {
    printf ("Min Brightness must be 50-254\n");
    exit(1);
  }
  printf ("Minimum brightness = %d\n", min_brightness);

  char blank_period[] = "00:00-00:00"; // default value
  printf ("blank_period default = %s\n", blank_period);
  strcpy(blank_period, argv[5]);
  printf ("blank_period defined = %s\n", blank_period);
  char delimiter[2] = ":-";

  char * temp = strtok(blank_period, delimiter);
  tm_blank.tm_hour = atoi(temp);

  temp = strtok(NULL, delimiter);
  tm_blank.tm_min = atoi(temp);

  temp = strtok(NULL, delimiter);
  tm_unblank.tm_hour = atoi(temp);

  temp = strtok(NULL, delimiter);
  tm_unblank.tm_min = atoi(temp);
  printf("blank_period = %d:%d until %d:%d\n",
          tm_blank.tm_hour, tm_blank.tm_min,
          tm_unblank.tm_hour, tm_unblank.tm_min);
  // see also: https://www.codingame.com/playgrounds/14213/how-to-play-with-strings-in-c/string-split

  int num_dev = argc - 6;
  int eventfd[num_dev];
  char device[num_dev][32];
  for (i = 0; i < num_dev; i++) {
    device[i][0] = '\0';
    strcat(device[i], "/dev/input/");
    strcat(device[i], argv[i + 6]);

    int event_dev = open(device[i], O_RDONLY | O_NONBLOCK);
    if(event_dev == -1){
      int err = errno;
      printf("Error opening %s: %d\n", device[i], err);
      exit(1);
    }
    eventfd[i] = event_dev;
  }
  printf("Using input device%s: ", (num_dev > 1) ? "s" : "");
  for (i = 0; i < num_dev; i++) {
    printf("%s ", device[i]);
  }

  printf("\n\nInitializing...\n");
  
  struct input_event event[64];
  FILE* brightfd;
  int event_size;
  int light_size;
  int size = sizeof(struct input_event);

  long int actual_brightness;
  long int max_brightness;
  long int current_brightness;
  
  /* char actual[53] = "/sys/class/backlight/rpi_backlight/actual_brightness";*/
  char max[50] = "/sys/class/backlight/rpi_backlight/max_brightness";
  char bright[46] = "/sys/class/backlight/rpi_backlight/brightness";

  brightfd = fopen(bright, "w");
  if(brightfd == NULL){
    int err = errno;
    printf("Error opening %s file: %d", bright, err);
    exit(1);
  }

  actual_brightness = 200;
  max_brightness = readint(max);
  current_brightness = actual_brightness;
  printf("actual_brightness %d, max_brightness %d\n", actual_brightness, max_brightness);

  printf("\n\nStarting...\n"); 
  time_t t_now = time(NULL);
  time_t t_last_touch = t_now;
  struct tm *tm_now;
  tm_now = localtime(&t_now);

  /*
   * current state:
   * 0 - initial state
   * 1 - screen active (screen is full bright)
   * 2 - slide_timeout reached (slide has been started)
   * 3 - dimmer_timeout reached (screen has been dimmed)
   * 10 - blank_time reached (screen has been blanked)
   * -1 - fatal error state (something critical went wrong)
   */
  int current_state = 0;
 
  pid_t slideshow_pid = -1;

  printf("Set brightness: %d\n", current_brightness);
  fprintf(brightfd, "%d\n", current_brightness);
  fflush(brightfd);
  fseek(brightfd, 0, SEEK_SET);
  /* TODO: add error handling of initializing */
  current_state = 1;
  
  /* running pi-touchscreen-controlling */
  while (current_state != -1) {

    t_now = time(NULL);
    tm_now = localtime(&t_now);

    // handling touch events
    for (i = 0; i < num_dev; i++){
      event_size = read(eventfd[i], event, size*64);
      if(event_size != -1) {
        /* STATE CHANGE: touch event raised */
        t_last_touch = t_now;
        /*
        Feature Request: Improve touch event handling
        Description:
        - tap: reset some state
               1. reset screen brightness from blank & dimmer events to photoframe
               2. reset paused photoframe to running photoframe
               3. reset screen to browser in kiosk mode
        - long press: pause photoframe
        (based on https://material.io/design/interaction/gestures.html#principles)

        Implementation ideas:
        - main procedure needs to be cleaned, should only handling events (touch and time)
        - create new "touchscreen-eventhandler"
        - create new "time-eventhandler"
        (based on https://ozzmaker.com/programming-a-touchscreen-on-the-raspberry-pi/)
        */
        // reset screen brightness from blank & dimmer events
        if (current_state > 2) {
          printf("%s Value: %d, Code: %x\n", device[i], event[0].value, event[0].code);

          current_brightness = actual_brightness;
          printf("Brightness now %d\n", current_brightness);
          fprintf(brightfd, "%d\n", current_brightness);
          fflush(brightfd);
          fseek(brightfd, 0, SEEK_SET);
          current_state = 2;
        }

        // stop slide
        if(current_state > 1) {
          printf("%s Value: %d, Code: %x\n", device[i], event[0].value, event[0].code);

          if( kill(slideshow_pid, SIGINT) == 0) {
            printf("Slide process %d has been stopped\n", slideshow_pid);

            int status;
            if ( waitpid(slideshow_pid, &status, 0) == -1) {
              printf("FATAL ERROR: Clean-up of slide process has not worked (waitpid).\n");
              /* stop program: zombie process could crahs the system */
              current_state = -1;
            } else {
              if ( WIFEXITED(status) ) {
                const int exit_status = WEXITSTATUS(status);
                printf("Slide process stopped with exit status %d\n", exit_status);
              }
              current_state = 1;
              slideshow_pid = -1;
            }
          } else {
            printf("FATAL ERROR: Slide process %d has not been stopped (kill).\n", slideshow_pid);
            /* stop program: zombie processes could crash the system */
            current_state = -1;
          }
        }
      }
    }

    // start slide if slide_timeout is over
    if (current_state == 1 && difftime(t_now, t_last_touch) > slide_timeout) {
      printf("STATE CHANGE: slide_timeout reached\n");
      current_state = 2;

      /* based on https://stackoverflow.com/questions/18530572/how-to-execute-a-command-from-c-and-terminate-it/18530798 */
      slideshow_pid = fork();
      if (slideshow_pid < 0) {
        printf("FATAL ERROR: fork returned %d\n", slideshow_pid);
        /* stop program: we don't know, if system could crash */
        current_state = -1;
      }
      if (slideshow_pid == 0) {
        // child process - will be replaced by slide
        printf("Slide will be started... (slide process id: %d)\n", slideshow_pid);
        /*
         * IMPROVE REQUEST: path to .Xauthority configurable or auto detectable
	 * Implementation ideas:
	 * - move child process from root context to 'main' user context (https://stackoverflow.com/questions/19048015/linux-c-programming-execute-as-user)
	 * - how to use as root the display of a normal user (https://www.pug.org/archive/mediawiki/index.php/Als_root_das_Display_des_Users_nutzen.html)
	 */
        char *env[] = { "DISPLAY=:0.0", "XAUTHORITY=/home/snarlhcu01/.Xauthority", (char *) NULL };
        execle("/usr/local/bin/slide",
               "slide",
               "-t", "60",
               "-p", slide_pictures,
               "-s",
               "-r",
               (char *) NULL, env);
         // child process - error happened during starting the child
         printf("FATAL ERROR: Slide did not correctly start - %s\n", strerror(errno));
         exit(EXIT_FAILURE);
      }
      // parent process
      printf("Slide has been started... (slide process id: %d)\n", slideshow_pid);
    }

    // dimm touchscreen if dimmer_timeout is over
    if(current_state == 2 && difftime(t_now, t_last_touch) > (slide_timeout + dimmer_timeout)) {
      printf("STATE CHANGE: dimmer_timeout reached\n");
      current_state = 3;
      while (current_brightness > min_brightness) {
        current_brightness -= 5;
        if (current_brightness < min_brightness) current_brightness = min_brightness;
        printf("Brightness now %d\n", current_brightness);
        fprintf(brightfd, "%d\n", current_brightness);
        fflush(brightfd);
        fseek(brightfd, 0, SEEK_SET);
        msleep(1000);
      }
    }

    // OLD BLANKING: if(current_state == 3 && difftime(t_now, t_last_touch) > (slide_timeout + dimmer_timeout + blank_timeout)) {
    // blank touchscreen if blank_time has been reached
    // (see also: https://www2.hs-fulda.de/~klingebiel/c-stdlib/time.htm)
    // Feature Request: Allow multiple time ranges for blanking (eg. night and working time)
    if(current_state < 10 && tm_now->tm_hour == tm_blank.tm_hour && tm_now->tm_min == tm_blank.tm_min) {
      printf("STATE CHANGE: blank_time reached\n");
      current_state = 10;
      actual_brightness = current_brightness;
      while (current_brightness != 0) {
        current_brightness -= 5;
        if (current_brightness < 0) current_brightness = 0;
        printf("Brightness now %d\n", current_brightness);
        fprintf(brightfd, "%d\n", current_brightness);
        fflush(brightfd);
        fseek(brightfd, 0, SEEK_SET);
        msleep(1000);
      }
    }

    // unblank touchscreen if touchscreen is still blank AND unblank_time has been reached
    if(current_state == 10 && tm_now->tm_hour == tm_unblank.tm_hour && tm_now->tm_min == tm_unblank.tm_min) {
      current_brightness = actual_brightness;
      printf("Brightness now %d\n", current_brightness);
      fprintf(brightfd, "%d\n", current_brightness);
      fflush(brightfd);
      fseek(brightfd, 0, SEEK_SET);
      current_state = 2;
    }

    msleep(30000); // sleep for 30 seconds
  } // while (current_state != -1)

  printf("STATE CHANGE: error happened... reset everything and exit");

  printf("Resetting brightness...\n");
  fprintf(brightfd, "%d\n", actual_brightness);
  fflush(brightfd);
  fseek(brightfd, 0, SEEK_SET);
  fclose(brightfd);

  printf("Killing child processes...\n");
  kill(slideshow_pid, SIGKILL);
  waitpid(slideshow_pid, NULL, 0);

  exit(EXIT_SUCCESS);
}
