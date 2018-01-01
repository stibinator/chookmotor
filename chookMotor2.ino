#include <JeeLib.h>

// // low power sleep library
ISR(WDT_vect) { Sleepy::watchdogEvent(); }


// ----pin assignment----
const int forward = 9; // to motor relay
const int backward = 10; // to motor relay
const int ERROR_LED = 8;
const int doorUpSensor = 5; // switch goes low when door is up
const int doorDownSensor = 4; // switch goes low when door down
const int lightMeter = 6; // to LDR module
const int lightMeterPower = 7; // vcc for light meter module
const bool UP = true;
const bool DOWN = false;

// ----more constants
const float doorTime = 60; // how long to wait for the door to close (seconds)
const int loopDelay = 60; // sleep time (seconds)
const int maxAttempts = 3;// how many times to try and fix errors
const int numSamples = 10; // how many samples to take when deciding on day/night status
const float motorLatency = 50.0; // how many ms between checking the door sensors to see if door is fully open/closed while raising/lowering

// ----globals----
int sampleNum = 0; // ring buffer index
int readings[numSamples]; // the array of light readings
bool isError = false; // global error state
int attempts = 0; // counts attempts to close the door on an error
int waitAnHour = 60; // wait loop for errors

// the setup function runs once when you press reset or power the board
void setup() {
  // Serial.begin(115200);
  // msg("chookrobot");
  pinMode(forward, OUTPUT);
  pinMode(backward, OUTPUT);
  pinMode(doorUpSensor, INPUT_PULLUP);
  pinMode(doorDownSensor, INPUT_PULLUP);
  pinMode(lightMeter, INPUT);
  pinMode(lightMeterPower, OUTPUT);
  pinMode(ERROR_LED, OUTPUT);
  powerDownMotor();
  // fill up the sample array
  for (int i = 0; i < numSamples; i++){
    getReading();
      // msg((String)readings[i]);
    if (readings[i]){
      flash(3,50);
    } else {
      flash(1,250);
    }
     delay(100);
  }
  // check the current conditions and set the door
  // checkDaylight returns true if daytime, up == true
  if(checkDaylight()){
    // msg("day");
    moveDoor(UP);

  } else {
    // msg("night");
    moveDoor(DOWN);
  }
  if (isError){
    flash(10,50);
  }
}

// ------------- the Main Hoo-Hah------------------
void loop() {
   // msg("main loop");
   // msg("isError: " + (String)isError);

  if (checkDaylight()){
    if (isDoor(UP)){
      // daytime, door open, nothing to do
      powerDownMotor();
    } else {
      // daytime door is down, time to open up
      if (isError){
        // error condition, handle it
        if (waitAnHour > 0){
          //hasn't been 60 cycles since the last attempt yet
          waitAnHour--;
        } else {
          // the hour is up,
          // have another go
          if (attempts < maxAttempts){
            isError = moveDoor(UP);
            attempts++;
          }
          // reset the counter
          waitAnHour = 60;
        }
      } else {
        // No error, proceed with opoening the door
        attempts = 0;
        isError = moveDoor(UP);
        //succesfully opened door, reset the error count
      }
    }
    //--------------end of the daylight part of the cycle-------------
  } else if (checkNightTime){
    if (isDoor(DOWN)){
      // nighttime, door closed, nothing to do
      powerDownMotor();
    } else {
      // nighttime door is up, time to close down
      if (isError){
        // error condition, only try shutting the door until attempts >= maxAttempts
        if (attempts < maxAttempts){
          isError = moveDoor(DOWN);
          attempts++;
        } else{
          // could call home if we had a web connection
          // can't do anything more
        }
      } else {
        // No error, proceed with closing the door
        attempts = 0;
        isError = moveDoor(DOWN);
      }
    }
    //-------------end of the nighttime part of the cycle-------------
  } // else{
    // NB there will be some time during the day when neither checkNightTime or checkDaylight is true
    // here is where you would handle that
    // }

    // feeble attention-getting device for errors
    if (isError){
      flash(10, 50);
    }
    // end of the main loop. Shut up and go to sleep for a while
    Sleepy::loseSomeTime(loopDelay*1000);
  }


  int getReading(){
    // record daylight conditions
    digitalWrite(lightMeterPower, HIGH);
    // LDR module needs some time to power up
    Sleepy::loseSomeTime(200);
    // the lightmeter module goes LOW when it's light
    readings[sampleNum] = (digitalRead(lightMeter) == LOW);
    // increment the index and roll it around if it's at the end
    sampleNum = (sampleNum + 1) % numSamples;
    // thelightmeterdrains power, so turn it off between readings
    digitalWrite(lightMeterPower, LOW);
  }

  void powerDownMotor(){
    // turn off motor and relays
    digitalWrite(forward, HIGH); // high de-energises the relays
    digitalWrite(backward, HIGH);
  }

  bool isDoor(bool UPDN){
    // returns true if door is fully up or down
    // Switch goes low when it's triggered
    bool swState;
    if (UPDN == UP){
      swState = digitalRead(doorUpSensor)==LOW;
    } else {
      swState = digitalRead(doorDownSensor)==LOW;
    }
    // msg("switch = " + (String)swState);
     // msg("isDoor:UP " + (String)swState);
    if (swState){
      // do this now, or it will have to wait until after the dalay in the outer loop
      powerDownMotor();
    }
    // msg("isdoor returns" + (String)swState);
    return swState;
  }


  bool moveDoor(bool UPDN){
    //turn on the motor untill the door open sensor is triggered
    // or until it times out
    if (UPDN == UP){
      // msg("Moving door up\n");

    } else {
      // msg("Moving door down\n");

    }
    int i = 0;
    // doorTime * 10 * 100ms pause = door time in seconds
    // msg("moveDoor logic returns " + (String)i + ", " + (String)doorTime + ", " + (String)motorLatency);
    while ((! isDoor(UPDN)) && (i < doorTime * 1000/motorLatency)){
      // if it hasn't raised the door within the set time something's wrong
      i++;
      if (UPDN == UP){
        // msg("^");

      } else {
        // msg("v");

      }
      motor(UPDN);
      // msg("waiting for door: " + (String)i);
      delay(motorLatency); // at most 0.1 seconds latency
    }
    powerDownMotor();

     // msg("raising door took: ");
     // msg((String)i);
    return((i >= doorTime)); // has the door timed out?
  }

  void motor(bool UPDN) {
    if (UPDN == UP){
      digitalWrite(forward, LOW);
      digitalWrite(backward, HIGH);
    } else {
      digitalWrite(forward, HIGH);
      digitalWrite(backward, LOW);
      //   digitalWrite(enable, HIGH);
    }
  }



  bool checkDaylight(){
     // msg("CheckDaylight");
    getReading();
    bool dayTime = false;
    // returns true unless all readings are false
    for(int i=0; i < numSamples; i++){
      dayTime = dayTime || readings[i];
    }

     // msg("dayTime: ");
     // msg((String)dayTime);
    return dayTime;
  }

  bool checkNightTime(){
     // msg("CheckNightTime");
    getReading();
    bool dayTime = true;
    // returns false unless all readings are true
    for(int i=0; i < numSamples; i++){
      dayTime = dayTime && readings[i];
    }
     // msg("dayTime: ");
     // msg((String)dayTime);
    return (! dayTime);
  }

  void flash(int num, int space){
    for (int f=0;f<num;f++){
      // flash the LED if error
      digitalWrite(ERROR_LED, HIGH);
      Sleepy::loseSomeTime(space);
      digitalWrite(ERROR_LED, LOW);
      Sleepy::loseSomeTime(space);
    }
  }

  // void msg(String payload){
  // Serial.print((String)payload + "\n"); // avoid clogging up the usb port
  // delay(200);
  // }
