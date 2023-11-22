#include <mbed.h>
#include "gyro.h"
#include "FlashIAPBlockDevice.h"
/* What this update does: read data (raw, integer) from gyroscope evrey 50ms
 * and if user button is pressed, save the most recent 512 datapoints (X-Y-Z)
 * to flash memory at SECTOR 15
 */
#define ADDR_FLASH_SEC15 0x0810'C000
// Create flash IAP block device, start at flash sector 15 address
// Sector 15 is 16KB, hence 16 * 1024 for size
FlashIAPBlockDevice bd(ADDR_FLASH_SEC15, 16 * 1024);

EventFlags flags;
Timeout t_out;
Timeout tbutton;
DigitalOut led1(LED1);
InterruptIn pb(BUTTON1, PullDown);
volatile bool sample_ready=false;
volatile bool button_pressed = false;
volatile bool check_buttonpressed = false;
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
  pb.rise(&button_cb);
  bd.init();
  GYRO_init();
  t_out.attach(sample, 50ms);
  

  while (1){
    int16_t raw_gx, raw_gy, raw_gz;

    // 50ms Polling for data sampling
    while(!sample_ready){
    }

    
    raw_gx = GYRO_X();
    raw_gy = GYRO_Y();
    raw_gz = GYRO_Z();
    XYZ_array[count] = raw_gx;
    XYZ_array[count + 512] = raw_gy;
    XYZ_array[count + 512 * 2] = raw_gz;

    printf(">x_axis: %d|g\n", raw_gx);
    printf(">y_axis: %d|g\n", raw_gy);
    printf(">z_axis: %d|g\n", raw_gz);
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