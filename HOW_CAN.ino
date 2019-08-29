// send stress data to the CAN bus 

// the CAN bus messages ID defined to be:
// ### 0x112 ###
// the time to establish base line defined to be:
// ### 5 seconds ###
// the msg priority (0-15 lower is higher priority) defined to be:
// ### 0x4 ###
#define BASE_LINE_TIME 5000
#define HOW_CAN_ID 0x112
#define HOW_MSG_PRIOR 0x4

// ### Important: how to get/sent op_code:                                            ###
// ### the CAN_FRAME data.high = op_code                                              ###
// ### if the op_code is OFF or ON, the CAN_FRAME data.low = the raw data ###
// ### else, the CAN_FRAME data.low = 0x0                                             ###
// Out msg: ID = 0x112
// first 4 bytes -
// 0x0 => OFF
// 0x1 => ON
// 0x2 => PERFORMING_BASE_LINE
// 0x3 => ERROR
#define OFF 0x0
#define ON 0x1
#define PERFORMING_BASE_LINE 0x2
#define _ERROR 0x3

// In msg: ID = 0x112
// first 4 bytes -
// 00 => PERFORM_BASE_LINE
#define PERFORM_BASE_LINE 0x0

// threshold - 
// if the diff between 'curr' and 'last' values < -THRESHOLD --> on
// if the diff between 'curr' and 'last' values > THRESHOLD --> off
// several THRESHOLD values for fine tuning 
#define THRESHOLD_1 200
#define THRESHOLD_2 120
// defining array size for calculating average of last ARR_SZ diff values
#define ARR_SZ 10


#include <HX711.h>
#include <due_can.h>
#include "variant.h"

// for output format
HX711 scale(7, 8);

// Leave defined if you use native port, comment if using programming port
// define Serial SerialUSB


// Global Variables
// 0 - off 1 - on
bool _status = 0;
long base_line = 0;
// for calculating elapsed time
unsigned long elapsed_time;
unsigned long current_time;
unsigned long start_time=0;
// for processing output
long curr_val;
long last_val;
// diff = |base - curr_val|
long diff;
bool diff_used = 0;
long last_vals[10];
long sum;
long average;
int count = 0;
// ----------------------------------------------------------------------------------------------


// Functions
void perform_base_line(unsigned long millisec)
{
  // for calculating average
  uint32_t count_samples = 0;
  uint32_t sum_value = 0;
  
  Serial.println("\nPERFORMING_BASE_LINE");

  // send "PERFORMING_BASE_LINE"
  sendData(HOW_CAN_ID, PERFORMING_BASE_LINE, 0x0);

  // calculate the base line
  start_time = millis();
  count_samples = 0;
  sum_value = 0;
  while(true)
  {
    // ### Debug Print ### 
    /*
    Serial.println(scale.read());
    */
    // for calculating average
    count_samples++;
    // ### Debug Print ###
    /*
    Serial.print("count_samples = ");
    Serial.println(count_samples);
    */
    sum_value+=abs(scale.read());
    // ### Debug Print ###
    /*
    Serial.print("sum_value = ");
    Serial.println(sum_value);
    */ 
    
    // for calculating time
    current_time = millis();
    elapsed_time = current_time - start_time;
    // ### Debug Print ###
    /*
    Serial.print("elapsed_time = ");
    Serial.println(elapsed_time);
    Serial.println();
    */
    if(elapsed_time > millisec)
    {
      break;
    }
  }
    Serial.print("sum_value = ");
    Serial.println(sum_value);
    Serial.print("count_samples = ");
    Serial.println(count_samples);
    base_line = sum_value/count_samples;
    Serial.print("base_line = ");
    Serial.println(base_line);
}


void sendData(uint32_t id, uint32_t high, uint32_t low)
{
  CAN_FRAME outgoing;
  outgoing.id = id;
  outgoing.extended = false;
  outgoing.priority = HOW_MSG_PRIOR;
    
  outgoing.data.high = high;
  outgoing.data.low = low;
  
  // Serial.println("outgoing.data.high:");
  // Serial.println(outgoing.data.high, HEX);
  // Serial.println("outgoing.data.low:");
  // Serial.println(outgoing.data.low, HEX);
  
  Can0.sendFrame(outgoing);
}

// push value to the array
// move all values 1 cell left (disposing arr[0]) and appending to the end new value 
void arr_push(uint32_t val)
{
  for(int i=0; i<ARR_SZ-1; i++)
  {
    last_vals[i]=last_vals[i+1];
  }
  last_vals[ARR_SZ-1]=val;
}
// ----------------------------------------------------------------------------------------------


void setup()
{
  Serial.begin(115200);
  // ### Debug Print ###
  /*
    Serial.println(scale.read());
  */
  // Initialize CAN0 and CAN1 
  // Set the proper baud rates here:
  // ### the DCAITI simulator BR is 500K ###
  if(Can0.begin(CAN_BPS_500K) && Can1.begin(CAN_BPS_500K))
  {
    Serial.println("CAN initialization Success");
  }
  else 
  {
    Serial.println("CAN initialization ERROR");
    sendData(HOW_CAN_ID, _ERROR, 0x0);
  }
  
  // listen for CAN msg
  Can0.watchFor();
  Can1.watchFor();
  
  // calculate the base line
  perform_base_line(BASE_LINE_TIME);

  curr_val = abs(scale.read());
}


void loop()
{
  // ### Debug Print ### 
  /*
  Serial.println(scale.read());
  */
  // listen for incoming msg
  CAN_FRAME incoming;
  
  // handle incoming requests:
  if (Can0.available() > 0) 
  {
    Can0.read(incoming);
    // only respones to HOW msg
    if(incoming.id == HOW_CAN_ID)
    {
      // handle "PERFORM_BASE_LINE" msg
      if(incoming.data.low == PERFORM_BASE_LINE)
      {
        // perform base line
        perform_base_line(BASE_LINE_TIME);
      }
      // handle other msg here
      // else if() ...
      // ...
      // ...
    }
  }
  
  // process and send data
  // the init status is 0 (off)
  // check diff for on/off indication
  last_val = curr_val;
  curr_val = abs(scale.read());
  diff = curr_val - last_val;
  // Serial.print("diff = ");
  // Serial.println(diff);

  // value decrease < -THRESHOLD --> off
  if(diff < -THRESHOLD_1)
  {
    _status = 1;
    diff_used = 1;
  }
  // value increase > THRESHOLD --> on
  else if(diff > THRESHOLD_1)
  {
    _status = 0;
    diff_used = 1;
  }
  
  // checak average for on/off indication
  // first fill the array for first time
  if(count < ARR_SZ)
  {
    last_vals[count] = diff/abs(diff);
    count++;
  }
  // if array is full calc values:
  else if (count >= ARR_SZ) // && (!diff_used))
  {
    sum = 0;
    for(int i=0; i<ARR_SZ; i++)
    {
      // sum += last_vals[i];
      sum += last_vals[i];
    }
    // average = sum/ARR_SZ;
    // if ARR_SZ -2 or more from last ARR_SZ signs are the same, assume on
    // usefull for stirring
    if(!diff_used && (sum >= ARR_SZ -2 || sum <= -ARR_SZ + 2))
    {
      // Serial.print("sum = ");
      // Serial.println(sum);
      _status = 1;
    }
    // "return to base"
    if(curr_val >= base_line - THRESHOLD_2)
    {
      // Serial.print("curr_val = ");
      // Serial.println(curr_val);      
      _status = 0;
    }
  }
  // reset the flag
  diff_used = 0;
  // push new value to aray (either array used or not)
  arr_push(diff/abs(diff));
  
  // send status message
  if(_status == 1)
  {
    sendData(HOW_CAN_ID, ON, curr_val);
  }
  else if(_status == 0)
  {
    sendData(HOW_CAN_ID, OFF, curr_val);
  }
}
