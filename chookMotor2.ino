#include <JeeLib.h>
#include <Arduino.h>
// // low power sleep library
ISR(WDT_vect)
{
  Sleepy::watchdogEvent();
}

// ----pin assignment----
const int testModeSwitch = 12;     //put the robot into test mode
const int serialEnableSwitch = 11; // talk on the serial bus
const int doorUpSensor = 4;        // switch goes low when door is up
const int doorDownSensor = 5;      // switch goes low when door down
const int lightMeter = 6;          // to LDR module
const int lightMeterPower = 7;     // to Vcc for light meter module, so we can power it up & down
const int ERROR_LED = 8;
const int forward = 9;   // to motor relay
const int backward = 10; // to motor relay

// ----more constants-----
const bool UP = true;
const bool DOWN = false;
const float doorTime = 12.0;      // how long to wait for the door to close (seconds)
const word loopDelay = 60;       // sleep time (seconds)
const int maxAttempts = 3;       // how many times to try and close/open door.
const int numSamples = 12;       // how many samples to take when deciding on day/night status
const float motorLatency = 5.0; // how many ms between checking the door sensors to see if door is fully open/closed while raising/lowering

#define DAYTIME numSamples
#define NIGHTIME 0

// ----global variables----
int sampleNum = 0;        // ring buffer index
int readings[numSamples]; // the array of light readings
bool isError = false;     // global error state
int attemptCount = 0;     // counts attempts to close the door on an error
// int waitAnHour = 60; // wait loop for errors

bool testMode()
{
  return digitalRead(testModeSwitch) == LOW;
}

bool serialEnabled()
{
  return digitalRead(serialEnableSwitch) == LOW;
}

void msg(String payload, bool serialIsEnabled)
{
  if (serialIsEnabled)
  {
    Serial.begin(9600);
    Serial.println(payload);
    Serial.end();
  }
}
void msg(int payload, bool serialIsEnabled)
{
  if (serialIsEnabled)
  {
    Serial.begin(9600);
    Serial.println((String)payload);
    Serial.end();
  }
}

void setup()
{

  pinMode(testModeSwitch, INPUT_PULLUP);
  pinMode(serialEnableSwitch, INPUT_PULLUP);
  msg(String("chookrobot 3.0"), serialEnabled());
  if (testMode())
  {
    msg("test mode", serialEnabled());
  }

  pinMode(forward, OUTPUT);
  pinMode(backward, OUTPUT);
  pinMode(doorUpSensor, INPUT_PULLUP);
  pinMode(doorDownSensor, INPUT_PULLUP);
  pinMode(lightMeter, INPUT);
  pinMode(lightMeterPower, OUTPUT);
  pinMode(ERROR_LED, OUTPUT);

  powerDownMotor();
  // fill up the sample array
  for (int i = 0; i < numSamples; i++)
  {
    getReading();
    if (readings[i])
    {
      flash(3, 50);
    }
    else
    {
      flash(1, 250);
    }
    if (testMode())
    {
      msg(readings[i], serialEnabled());
    }
    Sleepy::loseSomeTime(100);
  }
}

void loop()
{
  //check current conditions and set door accordingly

  if (testMode())
  {
    String result = "";
    flash(20, 50);
    //    msg("test mode\nraising door", serialEnabled());
    //    moveDoor(UP);
    if (isDoor(UP))
    {
      result += "door is  up ";
    }
    else
    {
      result += "door not up ";
    }

    //    delay(1000);
    //    msg("lowering door", serialEnabled());
    //    moveDoor(DOWN);
    if (isDoor(DOWN))
    {
      result += "door is  down ";
    }
    else
    {
      result += "door not down ";
    }
    //    msg("door down = " + result, serialEnabled());
    //    delay(1000);
    if (checkNightOrDay() == DAYTIME)
    {
      result += "daylight true  ";
    }
    else
    {
      result += "daylight false ";
    }
    //    msg("daylight = " + result, serialEnabled());
    //    delay(1000);
    if (checkNightOrDay() == NIGHTIME)
    {
      result += "night true ";
    }
    else
    {
      result += "night false ";
    }
    //    msg("night = " + result, serialEnabled());
    Sleepy::loseSomeTime(1000);
    if (isError)
    {
      result += "error true ";
    }
    else
    {
      result += "error false";
    }
    msg(result, serialEnabled());
  }
  else
  { // ---------------------------------------------normal operation
    getReading();
    int dayOrNight = checkNightOrDay();
    if (dayOrNight == DAYTIME)
    {
      msg("daylight", serialEnabled());
      // ------------- Day time ------------------
      if (isDoor(UP))
      {
        msg("door is up", serialEnabled());
        // door open, nothing to do
        powerDownMotor();
        attemptCount = 0;
        isError = false;
      }
      else
      {
        if (attemptCount <= maxAttempts)
        {
          // daytime but door is down, time to open up
          msg("moving door UP", serialEnabled());
          isError = moveDoor(UP);
          if (isError)
          {
            msg("error opening door", serialEnabled());
            attemptCount += 1;
            msg("attemptCount == " + String(attemptCount), serialEnabled());
          }
          else
          {
            msg("door moved up", serialEnabled());
            //succesfully opened door, reset the error count
            attemptCount = 0;
          }
        }
      }
      //--------------end of the daylight part of the cycle-------------
    }
    else if (dayOrNight == NIGHTIME)
    {
      // ------------- Night time ------------------
      if (isDoor(DOWN))
      {
        msg("door is down", serialEnabled());
        // nighttime, door closed, nothing to do
        powerDownMotor();
        isError = false;
        attemptCount = 0;
      }
      else
      {
        if (attemptCount <= maxAttempts)
        {
          // nighttime door is up, time to close down
          msg("moving door DOWN", serialEnabled());
          isError = moveDoor(DOWN);
          if (isError)
          {
            msg("error closing door", serialEnabled());
            attemptCount += 1;
            msg("attemptCount == " + String(attemptCount), serialEnabled());
          }
          else
          {
            msg("door moved down", serialEnabled());
            attemptCount = 0;
          }
        }
      }
      //-------------end of the nighttime part of the cycle-------------
    }
    else
    {
      // NB there will be some time during the day when neither checkNightTime or checkDaylight is true
      // here is where we handle that
      msg("niether night nor day", serialEnabled());
      powerDownMotor();
    }

    // feeble attention-getting device for errors
    if (isError)
    {
      msg("error.Help me someone", serialEnabled());
      flash(10, 50); //TODO put the LED where I can see it
    }
    // end of the main loop. Shut up and go to sleep for a while
    msg("sleeping", serialEnabled());
    Sleepy::loseSomeTime(loopDelay * 1000);
  }
}

void getReading()
{
  // record daylight conditions
  digitalWrite(lightMeterPower, HIGH);
  // LDR module needs some time to power up
  Sleepy::loseSomeTime(200);
  // the lightmeter module goes LOW when it's light
  readings[sampleNum] = (digitalRead(lightMeter) == LOW)? 1 : 0;
  // increment the index and roll it around if it's at the end
  sampleNum = (sampleNum + 1) % numSamples;
  // thelightmeterdrains power, so turn it off between readings
  digitalWrite(lightMeterPower, LOW);
}

void powerDownMotor()
{
  // turn off motor and relays
  digitalWrite(forward, HIGH); // high de-energises the relays
  digitalWrite(backward, HIGH);
  //turn off the indicator LED if it's on
  digitalWrite(ERROR_LED, LOW);
}

bool isDoor(bool UPDN)
{
  // returns true if door is fully up or down
  // Switch goes low when it's triggered
  bool swState;
  if (UPDN == UP)
  {
    swState = digitalRead(doorUpSensor) == LOW;
  }
  else
  {
    swState = digitalRead(doorDownSensor) == LOW;
  }
  if (swState)
  {
    // do this now, or it will have to wait until after the dalay in the outer loop
    powerDownMotor();
  }
  return swState;
}

bool serialEnable()
{
  return digitalRead(serialEnableSwitch) == LOW;
}

bool moveDoor(bool UPDN)
{
  //turn on the motor untill the door open sensor is triggered
  // or until it times out
  unsigned int i = 0;
  unsigned int maxDoorTime = doorTime * 1000 / motorLatency;
  // doorTime * 10 * 100ms pause = door time in seconds
  while ((!isDoor(UPDN)) && (i < maxDoorTime))
  {
    // il;jm;'lf it hasn't raised the door within the set time something's wrong
    i++;
    motor(UPDN);
    delay(motorLatency); // at most 0.1 seconds latency
  }
  powerDownMotor();

  msg(i, serialEnabled());
  return ((i >= maxDoorTime)); // has the door timed out?
}

void motor(bool UPDN)
{
  if (UPDN == UP)
  {
    digitalWrite(forward, LOW);
    digitalWrite(backward, HIGH);
    // turn on the LED
    digitalWrite(ERROR_LED, HIGH);
  }
  else
  {
    digitalWrite(forward, HIGH);
    digitalWrite(backward, LOW);
    //   digitalWrite(enable, HIGH);
  }
}

int checkNightOrDay(){
  int result= 0;
  for (int i = 0; i < numSamples; i++)
  {
    result += readings[i];
    msg(String("result = " + String(result)), serialEnabled());
  }
  return(result);
}

void flash(int num, int space)
{
  for (int f = 0; f < num; f++)
  {
    // flash the LED if error
    digitalWrite(ERROR_LED, HIGH);
    Sleepy::loseSomeTime(space);
    digitalWrite(ERROR_LED, LOW);
    Sleepy::loseSomeTime(space);
  }
}
