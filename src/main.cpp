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
#include "mbed.h"
#include "gyrometer.h"
#include <LCD_DISCO_F429ZI.h>
#include "FlashIAPBlockDevice.h"
#include <iomanip>

LCD_DISCO_F429ZI lcd;
#define ADDR_FLASH_SEC15 0x0810'C000
#define PI 3.141592
// Create flash IAP block device, start at flash sector 15 address
// Sector 15 is 16KB, hence 16 * 1024 for size
FlashIAPBlockDevice bd(ADDR_FLASH_SEC15, 16 * 1024);
// measure distance for one step given the data set and height of user (in meters)
float measureDistanceOneStep(int16_t gyro_x, int16_t gyro_y, int16_t gyro_z, float heightOfUser);
string float_to_String(float num);

EventFlags flags;
Timeout t_out;
Timeout tbutton;
DigitalOut led1(LED1);
InterruptIn pb(BUTTON1, PullDown);
volatile bool sample_ready=false;
volatile bool button_pressed = false;
volatile bool check_buttonpressed = false;
volatile float totalDist = 0.0;
volatile float distance = 0.0;

void sample(){
  sample_ready=true;
}

void spi_cb(int event){
  flags.set(SPI_FLAG);
}

// Push-button "pushed "ISR - needs a little debouncing
// so instead of asserting button pressed, here it sets a flag to check 
// the button again in 10ms
void button_cb(){
  check_buttonpressed = true;
}

// Push-button "timer" ISR - if the 10ms timer up and the pin is still
// HIGH, we assert it pressed
void button_check(){
  if (pb.read() & 0x01) {
    button_pressed = true;
  }else{
    button_pressed = false;
  }
}

int main() {
  int16_t XYZ_array[512 * 3];
  uint16_t count = 0;

  // Initializations
  pb.rise(&button_cb);
  bd.init();
  GYRO_init(); // threshold of 250dps is set on xyz axis
  t_out.attach(sample, 50ms);
  BSP_LCD_SetFont(&Font20);
  lcd.Clear(LCD_COLOR_BLACK);
  lcd.SetBackColor(LCD_COLOR_BLACK);
  lcd.SetTextColor(LCD_COLOR_BLUE);
  lcd.DisplayStringAt(0, LINE(4), (uint8_t *)"Total distance: ", CENTER_MODE);
  lcd.DisplayStringAt(0, LINE(5), (uint8_t *)"0.0 m", CENTER_MODE);
  
  while (1){
    int16_t raw_gx, raw_gy, raw_gz;

    // 50ms Polling for data sampling
    while(!sample_ready){
    }

    // Read data from gyroscope
    raw_gx = GYRO_X();
    raw_gy = GYRO_Y();
    raw_gz = GYRO_Z();
    XYZ_array[count] = raw_gx;
    XYZ_array[count + 512] = raw_gy;
    XYZ_array[count + 512 * 2] = raw_gz;

    // printf(">x_axis: %d|g\n", raw_gx);
    // printf(">y_axis: %d|g\n", raw_gy);
    // printf(">z_axis: %d|g\n", raw_gz);

    // calculate distance
    ::distance = measureDistanceOneStep(raw_gx, raw_gy, raw_gz, 1.7);
    ::totalDist = ::totalDist + ::distance;


    // Display on LCD
    uint8_t message1[30];
    sprintf((char *)message1, "%0.2f m", ::totalDist);
    lcd.DisplayStringAt(0, LINE(5), (uint8_t *)&message1, CENTER_MODE);
    // if (::distance - 0.001 < 0){
    //   lcd.DisplayStringAt(0, LINE(6), (uint8_t *)"Idling", CENTER_MODE);
    // }
    
    // Push Button1 on board (the blue one) to store data into flash
    count = (count + 1) % 512;
    if (button_pressed){
      bd.erase(0, bd.get_erase_size());
      bd.program(XYZ_array, 0, bd.get_erase_size());
      button_pressed = false;
    }
    
    // Reset 50ms timer 
    sample_ready=false;
    t_out.attach(sample, 50ms);
    
    if(!check_buttonpressed){
      continue;
    }else {
        // sets the 10ms check timer for pushbutton
      tbutton.attach(button_check, 10ms);
      check_buttonpressed = false;
    }
  }
}

float measureDistanceOneStep(int16_t gyro_x, int16_t gyro_y, int16_t gyro_z, float heightOfUser){
  float legLength = 0.55 * heightOfUser;
  float result = 0.0;
  int16_t gyro_z_abs = abs(gyro_z);
  if (gyro_z_abs > 4000){
    // Transform to angle velocity. 
    float degree = (0.00875 * (gyro_z_abs + 63));
    result =  ((float) 0.05) * (degree / 360) * 2 * PI * legLength;
  }
  return result;
}

string float_to_String(float num){
  // reserve 2 decimal places
  string num_str = to_string(num);
  return num_str.substr(0, num_str.find(".") + 3);
}