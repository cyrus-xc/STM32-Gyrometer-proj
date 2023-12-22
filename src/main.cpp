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
/* 12.11: 
 * Add speed recording system
 * The system record 20s of speed with 500ms eash
 * The screen shows the most recent maximum speed within a certain time period defined by SPEED_WINDOW currently set to 1500ms
 * Initialize data in the save() function 
 */
/* 12.16: 
 * Changed the button press to be the start of a 25 sec period (500 data points)
 * When the timer is up, output speed, instantaneous speed and total distance of the 25 sec period are saved
 * Added a threshold on peak so when gz is too low it start measuring
 * Changed the display to be instruct about the blue button function
 * Slightly changed the initializedata() function 
 * Changed the setting in CTRL_REG4 to set full scale to 245 dps (to match all the comments)
 * Corrected the scale factor to represent radius/sec
 * 
 * 12.21
 * Code clean up
 */
#include "mbed.h"
#include "gyrometer.h"
#include <LCD_DISCO_F429ZI.h>
#include <iomanip>
#include <stdio.h>
#include <errno.h>
#include <functional>
#include "HeapBlockDevice.h"
#include "FATFileSystem.h"
#include "USBMSD.h"


#define TABLE_SIZE  500
LCD_DISCO_F429ZI lcd;
#define PI 3.141592
// USB and FAT file system:
// Using mbed HeapBLockDevice class type
// Allocate RAM space for the file system to mount on
// Total 64kB storage
#define DEFAULT_BLOCK_SIZE  512
#define BUFFER_MAX_LEN 10
#define FORCE_REFORMAT true
// Define the USB Mass Storage Device using mbed library
// Connecting through CN6 (USB Micro-AB)
#define SPEED_LENGTH 40 //speed table length recording SPEED_LENGTH*50ms period of speed data
#define SPEED_WINDOW 3 //Using SPEED_WINDOW*500ms as speed showing window in the array

//output table struct
typedef struct{
    volatile float out_velocity[TABLE_SIZE];
    volatile float ins_velocity[TABLE_SIZE];
    volatile float dist[TABLE_SIZE];
    volatile uint16_t index;
} Table, *TablePTR;
// measure distance for one step given the data set and height of user (in meters)
float measureDistanceOneStep(int16_t gyro_z, float heightOfUser);
string float_to_String(float num);
BlockDevice *bd = new HeapBlockDevice(128 * DEFAULT_BLOCK_SIZE, 1, 1, 512);
// Using mbed FATFilesystem library and adapted sample code to define and handle file in the system
FATFileSystem fs("fs");
// Maximum number of elements in buffer
EventFlags flags;
Timeout t_sample;
Timeout t_record;
DigitalOut led1(LED1);
Table dist_table;
InterruptIn irq(BUTTON1);
volatile bool sample_ready=false;
volatile bool button_pressed = false;
volatile bool check_buttonpressed = false;
volatile float totalDist = 0.0;
volatile float distance = 0.0;
volatile int16_t peak = 0;
const int16_t peak_threshold = 4000;
const float scale_factor = PI / 180 * (8.75 / 1000); // 1 digit reading to radius/sec, 245 dps Full Scale, 8.75 being the sensitive parameter listed in datasheet
//Speed variables
float speed[SPEED_LENGTH];
float current_speed;
volatile uint16_t speed_counter=0;
volatile float previous_distance=0.0;

//reset sample_ready flag
void sample(){
  sample_ready=true;
}

//spi callback function reset flag
void spi_cb(int event){
  flags.set(SPI_FLAG);
}

//Initialize speed table to all 0.0
void speedinitialize()
{
  for(int i=0;i<SPEED_LENGTH;i++)
  {
    speed[i]=0.0;
  }
  ::previous_distance=0.0;
  ::speed_counter=0;
}

//Update the speed table, recording the most recent speed stores at the end of the table
void updatespeedtable()
{
  if(speed_counter>=500)
  {
    current_speed = (::totalDist-::previous_distance)/0.5;
    ::previous_distance=::totalDist;
    speed_counter=0;
    for(int i=0;i<SPEED_LENGTH-1;i++)
    {
        speed[i]=speed[i+1];
    }
    speed[SPEED_LENGTH-1]=current_speed;
    // move speed array to the left and add the new speed to the last element        
  }
  else
  {
    speed_counter+=50;
  }
}

//return the maximum speed in the  window size period, 
float getcurrentspeed()
{
  float tempspeed=0.0;
  for(int i=SPEED_LENGTH-SPEED_WINDOW-1;i<SPEED_LENGTH;i++)
  {
    if(tempspeed<speed[i])
      tempspeed=speed[i];
  }
  //ignore speed less than 0.05m/s
  if(tempspeed<=0.05)
  {
    tempspeed=0.0;
  }
  return tempspeed;
}

//Initializing all data
void initializedata()
{
  ::totalDist = 0.0;
  ::distance = 0.0;
  peak = 0;
  dist_table.index = 0;
  speedinitialize();
}

// Push-button "pushed "ISR
// Every time user button is pressed, a new data.csv
// file will be saved, (with the velocity and total distance) accessible through USB
void save() {
    int err;
    err = fs.mount(bd);
    if (err || FORCE_REFORMAT) {
        err = fs.reformat(bd);
        if (err) {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }
    FILE *f = fopen("/fs/data.csv", "w+");
    err = fprintf(f, "Output speed, Instant speed, Distance\n");
    if (!f) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }
    for (int i = 0; i < TABLE_SIZE; i++) {
        int j = (dist_table.index + i) % TABLE_SIZE;
        err = fprintf(f, "%f, %f, %f\n", dist_table.out_velocity[j], dist_table.ins_velocity[j], dist_table.dist[j]);
        if (err < 0) {
            error("error: %s (%d)\n", strerror(errno), -errno);
        }
    }
    err = fclose(f);
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    err = fs.unmount();
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(-err), err);
    }
    lcd.DisplayStringAt(0, LINE(7), (uint8_t *)"File Saved", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(8), (uint8_t *)"Push Blue Button", CENTER_MODE);
    lcd.DisplayStringAt(0, LINE(9), (uint8_t *)"To Record 25sec", CENTER_MODE);
}

static auto save_event = mbed_event_queue()->make_user_allocated_event(save);

//Blue button callback function
void button_cb(){
  initializedata();
  button_pressed = true;
  t_record.attach(std::ref(save_event),25.0);
}

// initizalize velocity table
void table_init(TablePTR tableptr){
    tableptr->index = 0;
    for(int i = 0; i < TABLE_SIZE; i++){
      tableptr->out_velocity[i] = 0;
      tableptr->ins_velocity[i] = 0;
      tableptr->dist[i] = 0;
    }
}

//update output velocity table
void table_update(TablePTR tableptr, float out, float instant, float totaldist){
    tableptr->out_velocity[tableptr->index] = out;
    tableptr->ins_velocity[tableptr->index] = instant;
    tableptr->dist[tableptr->index] = totaldist;
    tableptr->index = (tableptr->index + 1) % TABLE_SIZE;
}

//measureing each step distance
float measureDistanceOneStep(int16_t gyro_z, float heightOfUser){
  float legLength = 0.55 * heightOfUser;
  float result = 0.0;
  int16_t gyro_z_abs = abs(gyro_z);
  // float degree = gyro_z_abs * 8.75 / 1000 / 360;
  float degree = gyro_z_abs * scale_factor * 0.05;
  // calculate distance:
  // 0.05 is the time interval between each sample
  // degree / 360 is the ratio of the angle of one sample to a full circle
  // 2 * legLength * sin(degree / 2) is the circumference of the circle
  result = 2 * legLength * sin(degree / 2);
  // result =  0.05 * (degree / 360) * 2 * PI * legLength;
  return result;
}

//convert float to string 
string float_to_String(float num){
  // reserve 2 decimal places
  string num_str = to_string(num);
  return num_str.substr(0, num_str.find(".") + 3);
}

int main() {
  const uint8_t rawbuf_size = 32;
  int16_t rawX_array[rawbuf_size];
  int16_t rawY_array[rawbuf_size];
  int16_t rawZ_array[rawbuf_size];
  uint16_t count = 0;
  table_init(&dist_table);
  speedinitialize();
  // Initializations
  bd->init();
  GYRO_init(); // threshold of 245dps is set on xyz axis
  // Setup the save event on button press, use the event queue
  // to avoid running in interrupt context
  irq.fall(button_cb);
  // Initialized USB
  USBMSD usb(bd);
  t_sample.attach(sample, 50ms);
  BSP_LCD_SetFont(&Font20);
  lcd.Clear(LCD_COLOR_BLACK);
  lcd.SetBackColor(LCD_COLOR_BLACK);
  lcd.SetTextColor(LCD_COLOR_BLUE);
  lcd.DisplayStringAt(0, LINE(2), (uint8_t *)"Current Speed: ", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(3), (uint8_t *)"0.0 m/s", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(4), (uint8_t *)"Total distance: ", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"0.0 m", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(8), (uint8_t *)"Push Blue Button", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(9), (uint8_t *)"To Record 25sec", CENTER_MODE);
  
  while (1){
    int16_t raw_gx, raw_gy, raw_gz;
    usb.process();

    // 50ms Polling for data sampling, non-blocking
    if(sample_ready){
      // Read data from gyroscope
      raw_gx = GYRO_X();
      raw_gy = GYRO_Y();
      raw_gz = GYRO_Z();
      peak = peak > abs(raw_gz) ? peak : abs(raw_gz);
      peak = peak > peak_threshold ? peak : peak_threshold;
      rawX_array[count] = raw_gx;
      rawY_array[count] = raw_gy;
      rawZ_array[count] = raw_gz;

      // calculate distance when we hit over 5% of the peak
      if (abs(raw_gz) > peak * 0.05){
        ::distance = measureDistanceOneStep(raw_gz, 1.7);
        ::totalDist = ::totalDist + ::distance;
      }
    
      // Display on LCD
      uint8_t message1[30];
      sprintf((char *)message1, "%6.2f m", ::totalDist);
      lcd.DisplayStringAt(0, LINE(5), (uint8_t *)&message1, CENTER_MODE);

      //Speed recording and showing
      updatespeedtable();

      //Always showing the max speed in the recent 1500ms
      float output_speed = getcurrentspeed();
      uint8_t message2[30];
      sprintf((char *)message2, "%6.2f m/s", output_speed);
      lcd.DisplayStringAt(0, LINE(3), (uint8_t *)&message2, CENTER_MODE);
      table_update(&dist_table, output_speed, current_speed, ::totalDist);

      //blue button record saving and reset function
      if (button_pressed){
        lcd.ClearStringLine(7);
        lcd.DisplayStringAt(0, LINE(8), (uint8_t *)"Recording...... ", CENTER_MODE);
        lcd.ClearStringLine(9);
        button_pressed = false;
      }

      // Reset 50ms timer 
      sample_ready=false;
      t_sample.attach(sample, 50ms);
    }
  }
}