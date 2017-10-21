//low power sleep library
#include <JeeLib.h>

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

// ----pin assignment----
const int forward = 3; // connects to motor relay
const int backward = 2; // ' '
const int downSensor = 4; // switch to sense when door is closed
const int upSensor = 5; // switch to sense door fully up
const int lightMeter = 6; // LDR module digital out
const int lightMeterPower = 7; // vcc on LDR module. Turn off when not making readings to save power
const int manualUpSwitch = 8; // 3 position switch for manual override
const int manualDownSwitch = 9; // ' '

// ----more constants
const int numSamples = 10; // take numerous samples to avoid shadows or clouds triggering door closing
// ----globals----
int sampleNum = 0; // global counter for current reading
int readings[numSamples]; // the readings. Acts as a ring buffer
bool noError = true; // global error state
// the setup function runs once when you press reset or power the board
void setup(){
  // Serial.begin(115200); //for debug
  // assign pins
  pinMode(forward, OUTPUT);
  pinMode(backward, OUTPUT);
  pinMode(upSensor, INPUT_PULLUP);
  pinMode(downSensor, INPUT_PULLUP);
  pinMode(manualUpSwitch, INPUT_PULLUP);
  pinMode(manualDownSwitch, INPUT_PULLUP);
  pinMode(lightMeter, INPUT);
  pinMode(lightMeterPower, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  // power down the motor and relay
  motorStop();
  // fill up the sample array
  for (int i = 0; i < numSamples; i++){
    getReading();
    delay(1000);
  }
  //check the current conditions and set the door
  //if manual up switch is set, or it's daylight and the manual down switch is not set, raise the door
  if (manualUp() || (checkDaylight() && ! manualDown())){
    //wake up, girls
    noError = raiseDoor();
  }
  else {
    //manual up switch is not set and either it's dark or the manual down switch is set
    noError = lowerDoor();
  }
}

void loop() {
  // delay(200); // was getting serial erros without delay between calls
  // ----------check for manual override-------
  if (manualUp()){
    noError = raiseDoor();
    Sleepy::loseSomeTime(10000);
  }
  else if (manualDown()) {
    noError = lowerDoor();
    Sleepy::loseSomeTime(10000);
  }
  else {
    // switch in auto position, do readings and react
    if (isDoorUp()){
      // delay(200);
      while (checkDaylight()){
        // if door is up and it's day time
        motorStop(); //just in case
        // do nothing for a bit
        Sleepy::loseSomeTime(60000);
      }
      // no longer daylight, good night girls
      noError = lowerDoor();
    }
    else if (isDoorDown()){
      // delay(200);
      while (checkNightTime()){
        // door is down and it's night time
        //stop everything and sleep for a bit
        motorStop();
        Sleepy::loseSomeTime(60000);
      }
      //stopped being night time
      noError = raiseDoor();
    }
    else {
      //something has gone wrong, door has failed to open or close
      noError = false;
    }
  }
  if (! noError){
    // might put a wifi module on if needs be
    // would call home now, but instead
    // flash the LED if error
    for (int j=0;j<10;j++){
      digitalWrite(LED_BUILTIN, HIGH);
      Sleepy::loseSomeTime(400);
      digitalWrite(LED_BUILTIN, LOW);
      Sleepy::loseSomeTime(400);
    }
  }
}


int getReading(){
  // poll the LDR module
  digitalWrite(lightMeterPower, HIGH);
  // LDR needs some time to power up
  Sleepy::loseSomeTime(200);
  //the lightmeter module goes LOW when it detects light
  readings[sampleNum] = (digitalRead(lightMeter) == LOW);
  //thelightmeterdrains power, so turn it off between readings
  digitalWrite(lightMeterPower, LOW);
  // write to the ring buffer
  sampleNum = (sampleNum + 1) % numSamples;
}

void motorStop(){
  // high de-energises the relay
  // this shorts across the motor so that it brakes
  digitalWrite(forward, HIGH); s
  digitalWrite(backward, HIGH);
}

bool isDoorUp(){
  // check to see if the door is fully up
  bool swState = ! (digitalRead(upSensor));
  if (swState){
    //do this now, or it will have to wait until after the dalay in the outer loop
    motorStop();
  }
  // else {
  // }
  return swState;
}

bool isDoorDown(){
  bool swState = (! digitalRead(downSensor));
  if (swState){
    motorStop();
  }
  return swState;
}

// check to see if the switch is in the up position
bool manualUp(){
  return digitalRead(manualUpSwitch);
}

bool manualDown(){
  return digitalRead(manualDownSwitch);
}

// runs the motor untill the up sensor is triggered, or it times out
// returns true on successful completion
void raiseDoor(){
  int i = 0; // timeout counter
  //if it hasn't raised the door within a minute something's wrong
  while (! isDoorUp() && i < 600){
    i+=1;
    motorForward();
    delay(100);
  }
  motorStop();
  return(i < 600); // if the door timed out return true
}

void lowerDoor(){
  //returns true on successful completion
  int i = 0;
  //if it hasn't lowered the door within a minute something's wrong
  while (! isDoorUp() && i < 600){
    i+=1;
    motorReverse();
    delay(100);
  }
  motorStop();
  return(i < 600);
}

void motorForward() {
  digitalWrite(forward, LOW);
  digitalWrite(backward, HIGH);
}

void motorReverse() {
  digitalWrite(forward, HIGH);
  digitalWrite(backward, LOW);
}



bool checkDaylight(){
  //returns true unless the last ten readings have detected night
  // take a reading
  getReading(); // returns true for day false for night
  // check all the readings
  int i=0;
  //if any one of the samples are true this will return true
  while ((i < numSamples - 1) && (! readings[i]);
  {
    i++;
  }
  // the counter only reaches the end of the samples
  // if all of the readings were false
  // so the last sample will either be true -> return true
  // or all of the samples were false including last one -> return false
  return readings[i];
}

bool checkNightTime(){
  //returns true unless the last ten readings have detected daylight
  getReading();
  //returns false unless all readings are true
  int i=0;
  while ((i < numSamples-1) && readings[i])
  {
    i++;
  }
  // loop finishes as soon as it hits a day time reading -> returns false
  // so the last reading will be false if it's night time -> returns true
  return ! readings[i];
}
