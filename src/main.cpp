/* 11.20: read data (raw, integer) from gyroscope evrey 50ms
 * and if user button is pressed, save the most recent 512 datapoints (X-Y-Z)
 * to flash memory at SECTOR 15
 */
/* 11.26: 
 * Added calculate distance algorithm
 * Added LCD display support
 * Enabled float point printf for mbed
 * Changed gyro init to a 250dps threshold
 */
/* 12.3: 
 * New distance measurement algorithm
 */
/* 12.4: 
 * Reduced the raw sensor data array to 32 - should be enough for any filter
 * Saved a 512 datapoint array of calculated velocity and distance (float)
 * Changed the underlying memory block from flash to RAM
 * Added file system and USB accessibility
 */
#include "mbed.h"
#include "gyrometer.h"
#include <LCD_DISCO_F429ZI.h>
#include <iomanip>
#include <stdio.h>
#include <errno.h>
#include <functional>

LCD_DISCO_F429ZI lcd;
#define PI 3.141592
// USB and FAT file system:
// Using mbed HeapBLockDevice class type
// Allocate RAM space for the file system to mount on
// Total 64kB storage
#include "HeapBlockDevice.h"
#define DEFAULT_BLOCK_SIZE  512
BlockDevice *bd = new HeapBlockDevice(128 * DEFAULT_BLOCK_SIZE, 1, 1, 512);
// Using mbed FATFilesystem library and adapted sample code 
// to define and handle file in the system
#include "FATFileSystem.h"
FATFileSystem fs("fs");
// Maximum number of elements in buffer
#define BUFFER_MAX_LEN 10
#define FORCE_REFORMAT true
// Define the USB Mass Storage Device using mbed library
// Connecting through CN6 (USB Micro-AB)
#include "USBMSD.h"

// measure distance for one step given the data set and height of user (in meters)
float measureDistanceOneStep(int16_t gyro_z, float heightOfUser);
string float_to_String(float num);

EventFlags flags;
Timeout t_out;
Timeout tbutton;
DigitalOut led1(LED1);

volatile bool sample_ready=false;
volatile bool button_pressed = false;
volatile bool check_buttonpressed = false;
volatile float totalDist = 0.0;
volatile float distance = 0.0;
volatile int16_t peak = 0;

void initializedata()
{
  sample_ready=false;
  button_pressed = false;
  check_buttonpressed = false;
  ::totalDist = 0.0;
  ::distance = 0.0;
  peak = 0;
}


#define TABLE_SIZE  512
typedef struct{
    volatile float velocity[TABLE_SIZE];
    volatile float dist[TABLE_SIZE];
    volatile uint16_t index;
} Table, *TablePTR;

Table dist_table;
void sample(){
  sample_ready=true;
}

void spi_cb(int event){
  flags.set(SPI_FLAG);
}

// Push-button "pushed "ISR - TBD a little debouncing
// For now, every time user button is pressed, a new data.csv
// file will be saved, (with the velocity and total distance) accessible through USB
// Still need to refine: 
// TBD: 
// 1.Bebouncing
// 2.Push indicate the start of "read" - clears the total distance
// and save the data when the 512 datapoints in table is full
InterruptIn irq(BUTTON1);
void save() {
    initializedata();
    int err;
    // Try to mount the filesystem
    // printf("Mounting the filesystem... ");
    // fflush(stdout);
    err = fs.mount(bd);
    // printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err || FORCE_REFORMAT) {
        // Reformat if we can't mount the filesystem
        // printf("formatting... ");
        // fflush(stdout);
        err = fs.reformat(bd);
        // printf("%s\n", (err ? "Fail :(" : "OK"));
        if (err) {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }
    // printf("Opening \"/fs/data.csv\"... ");
    // fflush(stdout);
    FILE *f = fopen("/fs/data.csv", "w+");
    // printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }
    for (int i = 0; i < TABLE_SIZE; i++) {
        int j = (dist_table.index + i) % TABLE_SIZE;
        // printf("\rWriting numbers (%d/%d)... ", i, TABLE_SIZE);
        // fflush(stdout);
        err = fprintf(f, "%f, %f\n", dist_table.velocity[j], dist_table.dist[j]);
        if (err < 0) {
            printf("Fail :(\n");
            error("error: %s (%d)\n", strerror(errno), -errno);
        }
    }
    // fflush(stdout);
    err = fclose(f);
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }
    // Tidy up
    // printf("Unmounting... ");
    // fflush(stdout);
    err = fs.unmount();
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(-err), err);
    }
}
static auto save_event = mbed_event_queue()->make_user_allocated_event(save);
void table_init(TablePTR tableptr){
    tableptr->index = 0;
    for(int i = 0; i < TABLE_SIZE; i++){
      tableptr->velocity[i] = 0;
      tableptr->dist[i] = 0;
    }
}

void table_update(TablePTR tableptr, float vel, float totaldist){
    tableptr->velocity[tableptr->index] = vel;
    tableptr->dist[tableptr->index] = totaldist;
    tableptr->index = (tableptr->index + 1) % TABLE_SIZE;
}

int main() {
  const uint8_t rawbuf_size = 32;
  int16_t XYZ_array[rawbuf_size * 3];
  uint16_t count = 0;
  table_init(&dist_table);
  // Initializations

  bd->init();
  GYRO_init(); // threshold of 250dps is set on xyz axis
  // Setup the save event on button press, use the event queue
  // to avoid running in interrupt context
  irq.fall(std::ref(save_event));
  // Initialized USB
  USBMSD usb(bd);
  t_out.attach(sample, 50ms);
  BSP_LCD_SetFont(&Font20);
  lcd.Clear(LCD_COLOR_BLACK);
  lcd.SetBackColor(LCD_COLOR_BLACK);
  lcd.SetTextColor(LCD_COLOR_BLUE);
  lcd.DisplayStringAt(0, LINE(4), (uint8_t *)"Total distance: ", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"0.0 m", CENTER_MODE);
  
  while (1){
    int16_t raw_gx, raw_gy, raw_gz;
    usb.process();
    // 50ms Polling for data sampling, non-blocking
    if(sample_ready){
      // Read data from gyroscope
      raw_gx = GYRO_X();
      raw_gy = GYRO_Y();
      raw_gz = GYRO_Z();
      peak = peak > abs(raw_gz)? peak : abs(raw_gz);
      XYZ_array[count] = raw_gx;
      XYZ_array[count + rawbuf_size] = raw_gy;
      XYZ_array[count + rawbuf_size * 2] = raw_gz;

      // Serial out put for debugging
      // printf(">x_axis: %d|g\n", raw_gx);
      // printf(">y_axis: %d|g\n", raw_gy);
      // printf(">z_axis: %d|g\n", raw_gz);

      // calculate distance when we hit over 5% of the peak
      if (abs(raw_gz) > peak * 0.05){
        ::distance = measureDistanceOneStep(raw_gz, 1.7);
        ::totalDist = ::totalDist + ::distance;
      }
      table_update(&dist_table, ::distance, ::totalDist);
    
      // Display on LCD
      uint8_t message1[30];
      sprintf((char *)message1, "%0.2f m", ::totalDist);
      lcd.DisplayStringAt(0, LINE(5), (uint8_t *)&message1, CENTER_MODE);
      // if (::distance - 0.001 < 0){
      //   lcd.DisplayStringAt(0, LINE(6), (uint8_t *)"Idling", CENTER_MODE);
      // }
      // Reset 50ms timer 
      sample_ready=false;
      t_out.attach(sample, 50ms);
    }
  }

    

}

float measureDistanceOneStep(int16_t gyro_z, float heightOfUser){
  float legLength = 0.55 * heightOfUser;
  float result = 0.0;
  int16_t gyro_z_abs = abs(gyro_z);
  float degree = gyro_z_abs * 8.75 / 1000 / 360;
  // calculate distance:
  // 0.05 is the time interval between each sample
  // degree / 360 is the ratio of the angle of one sample to a full circle
  // 2 * legLength * sin(degree / 2) is the circumference of the circle
  result = 2 * legLength * sin(degree / 2);
  // result =  0.05 * (degree / 360) * 2 * PI * legLength;
  return result;
}

string float_to_String(float num){
  // reserve 2 decimal places
  string num_str = to_string(num);
  return num_str.substr(0, num_str.find(".") + 3);
}