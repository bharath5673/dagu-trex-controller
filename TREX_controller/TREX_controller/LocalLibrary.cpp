#include "LocalLibrary.h"

ControllerInterface::ControllerInterface(ros::NodeHandle *nodeHandle) :
    RCdeadband(35),
    mNodeHandle(nodeHandle)
{
    servopin[0] = 7; servopin[1] = 8; servopin[2] = 12;
    servopin[3] = 13; servopin[4] = 5; servopin[5] = 6;
}

void ControllerInterface::Accelerometer()
{
    static int oldx, oldy, oldz, vibration;        // local variables used for impact detection
    
    oldx = xaxis;                                  // store previous accelerometer readings for comparison
    oldy = yaxis;
    oldz = zaxis;
    
    vibration--;                                   // loop counter prevents false triggering cause by impact vibration
    if(vibration < 0)
        vibration = 0;                             // as robot vibrates due to impact
    
    xaxis = analogRead( axisxpin );                // read accelerometer - note analog read takes 260uS for each axis
    yaxis = analogRead( axisypin );
    zaxis = analogRead( axiszpin );
    if(vibration > 0)
        return;                                    // until vibration has subsided no further calculations required
    
    deltx = xaxis - oldx;                          // difference between old and new axis readings
    delty = yaxis - oldy;
    deltz = zaxis - oldz;
    magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz)); // magnitude of delta x,y,z using Pythagorus's Theorum
    
    if(magnitude > sensitivity)                     // has a new impact occured
    {
        mNodeHandle->loginfo("(T'REX Controller) Impact Detected!");
        vibration = devibrate;                      // reset anti-vibration counter
        return;
    }
    else
    {
        magnitude = 0;                              // no impact detected
        deltx = 0;
        delty = 0;
        deltz = 0;
    }
}

void ControllerInterface::EmptyBuffer()
{
    byte b;
    while(Serial.available())                                    // empty buffer once response is received
    {
        b=Serial.read();
    }
}

void ControllerInterface::DiagnosticMode()
{
    //---------------------------------------- Diagnostic Mode ------------------------------------//
    //  This simple routine runs the motors forward / backward and brakes to test the "H" bridges  //
    //  Battery voltage, accelerometer data and motor current draw is displayed on serial monitor  //
    //             When LEDs are connected to servo outputs they will chase in sequence            //
    //---------------------------------------------------------------------------------------------//
    
    static int mdir,mpwm,brk,LED,div;
    if(mdir==0)                             // first time through the loop mdir=0
    {                                       // initialize diagnostic routine
        mdir=5;                               // motor direction cannot start at 0 or motors will not move
        for(byte i=0;i<6;i++)                 // scan through servo pins
        {
            pinMode(servopin[i],OUTPUT);        // set servo pin to OUTPUT
        }
    }
    
    mpwm+=mdir;                             // motor speed/direction is incremented/decremented by motor direction
    if(mpwm<-250 || mpwm>250)               // PWM value must be between -255 and +255
    {
        mdir=-mdir;                           // when limit is reached, reverse direction
        brk=1;                                // engage brake for quick slow down
    }
    if(mpwm==0) brk=0;                      // if motor PWM is 0 then disable the brake - motor can start again
    
    lmspeed=mpwm;                           // set left  motor speed
    rmspeed=mpwm;                           // set right motor speed
    lmbrake=brk;                            // enable / disable left  brake
    rmbrake=brk;                            // enable / disable right brake
    Motors();                               // update speed, direction and brake of left and right motors
    
    div++;                                  // divider used to slow down LED chasing
    if(div>20)
    {
        div=0;                                // reset divider
        LED++;                                // increment LED chase sequence
    }
    
    if(LED>5) LED=0;                        // cause chase sequence to repeat
    for(byte i=0;i<6;i++)                   // scan servo control pins
    {
        digitalWrite(servopin[i],LED==i);     // drive LEDs in chase sequence
    }

    delay(10);
}

void ControllerInterface::Encoders()
{
    static byte lencold,lencnew,rencold,rencnew;                      // old encoder values
    
    lencold = lencnew;                                                  // store previous value for left  encoder
    rencold = rencnew;                                                  // store previous value for right encoder
    lencnew = digitalRead(lmencpin);                                    // read new left  encoder value
    rencnew = digitalRead(rmencpin);                                    // read new right encoder value
    if(lencold != lencnew)
        lmenc += lmspeed / abs(lmspeed);                // if old does not equal new then add or subtract 1 depending on motor direction
    if(rencold != rencnew)
        rmenc += lmspeed / abs(lmspeed);                // if old does not equal new then add or subtract 1 depending on motor direction
}

void ControllerInterface::Motors()
{
    digitalWrite(lmbrkpin,lmbrake>0);                     // if left brake>0 then engage electronic braking for left motor
    digitalWrite(lmdirpin,lmspeed>0);                     // if left speed>0 then left motor direction is forward else reverse
    analogWrite (lmpwmpin,abs(lmspeed));                  // set left PWM to absolute value of left speed - if brake is engaged then PWM controls braking
    if(lmbrake>0 && lmspeed==0) lmenc=0;                  // if left brake is enabled and left speed=0 then reset left encoder counter
    
    digitalWrite(rmbrkpin,rmbrake>0);                     // if right brake>0 then engage electronic braking for right motor
    digitalWrite(rmdirpin,rmspeed>0);                     // if right speed>0 then right motor direction is forward else reverse
    analogWrite (rmpwmpin,abs(rmspeed));                  // set right PWM to absolute value of right speed - if brake is engaged then PWM controls braking
    if(rmbrake>0 && rmspeed==0) rmenc=0;                  // if right brake is enabled and right speed=0 then reset right encoder counter
}

void ControllerInterface::MotorBeep(byte beeps)
{
    digitalWrite(lmbrkpin,0);                             // ensure breaks are off
    digitalWrite(rmbrkpin,0);
    
    for(int b=0;b<beeps;b++)                              // loop to generate multiple beeps
    {
        for(int duration=0;duration<400;duration++)         // generate 2kHz tone for 200mS
        {
            digitalWrite(lmdirpin,1);                         // drive left  motor forward
            digitalWrite(rmdirpin,1);                         // drive right motor forward
            digitalWrite(lmpwmpin,1);                         // left  motor at 100%
            digitalWrite(rmpwmpin,1);                         // right motor at 100%
            delayMicroseconds(50);                            // limit full power to 50uS
            digitalWrite(lmpwmpin,0);                         // shutdown left  motor
            digitalWrite(rmpwmpin,0);                         // shutdown right motor
            delayMicroseconds(200);                           // wait aditional 200uS to generate 2kHz tone
            
            digitalWrite(lmdirpin,0);                         // drive left  motor backward
            digitalWrite(rmdirpin,0);                         // drive right motor backward
            digitalWrite(lmpwmpin,1);                         // left  motor at 100%
            digitalWrite(rmpwmpin,1);                         // right motor at 100%
            delayMicroseconds(50);                            // limit full power to 50uS
            digitalWrite(lmpwmpin,0);                         // shutdown left  motor
            digitalWrite(rmpwmpin,0);                         // shutdown right motor
            delayMicroseconds(200);                           // wait aditional 200uS to generate 2kHz tone
        }
        delay(200);                                         // pause for 200mS (1/5th of a second) between beeps
    }
}

void ControllerInterface::RCmode()
{
    //------------------------------------------------------------ Code for RC inputs ---------------------------------------------------------
    
    int Speed=int(pulseIn(RCspeedpin,HIGH,25000));              // read throttle/left stick
    int Steer=int(pulseIn(RCsteerpin,HIGH,25000));              // read steering/right stick
    
    if (Speed==0) Speed=1500;                                   // if pulseIn times out (25mS) then set speed to stop
    if (Steer==0) Steer=1500;                                   // if pulseIn times out (25mS) then set steer to centre
    
    if (abs(Speed-1500)<RCdeadband) Speed=1500;                 // if Speed input is within deadband set to 1500 (1500uS=center position for most servos)
    if (abs(Steer-1500)<RCdeadband) Steer=1500;                 // if Steer input is within deadband set to 1500 (1500uS=center position for most servos)
    
    Steer=Steer-1500;
    lmspeed=(Speed-Steer-1500)*8/10;
    rmspeed=(Speed+Steer-1500)*8/10;
    
    if(lmspeed<-255) lmspeed=-255;
    if(lmspeed>255)  lmspeed=255;
    if(rmspeed<-255) rmspeed=-255;
    if(rmspeed>255)  rmspeed=255;
    
    Motors();
}

void ControllerInterface::Servos()
{
    for(byte i=0;i<6;i++)                                                        // up to 6 servos are supported by T'REX controller
    {
        if(servopos[i]!=0 && servo[i].attached()==0) servo[i].attach(servopin[i]); // if servopos is non zero and servo is not attached then attach the servo
        if(servopos[i]==0 && servo[i].attached()!=0)                               // if servo pos is 0 but servo is attached
        {
            servo[i].detach();                                                       // if servopos=0 and the servo is attached then detach the servo
            pinMode(servopin[i],INPUT);                                              // set unused servo pin as input
        }
        
        if(servopos[i]>0) servo[i].writeMicroseconds(servopos[i]);                 // if servopos is >0 then move servo to that position
        if(servopos[i]<0) servo[i].writeMicroseconds(3000+servopos[i]);            // if servopos is <0 then move servo to that position but reverse sense of direction
    }
}

void ControllerInterface::Shutdown()
{
    //================ Shutdown motors and servos when battery is flat ==============
    mNodeHandle->loginfo("(T'REX Controller) Shutting down motors and servos...");
    
    lmspeed=0;      // set left  motor speed to 0 (off)
    rmspeed=0;      // set right motor speed to 0 (off)
    Motors();       // update H bridges
    
    servopos[0]=0;  // ensure servo 0 is detached
    servopos[1]=0;  // ensure servo 1 is detached
    servopos[2]=0;  // ensure servo 2 is detached
    servopos[3]=0;  // ensure servo 3 is detached
    servopos[4]=0;  // ensure servo 4 is detached
    servopos[5]=0;  // ensure servo 5 is detached
    Servos();       // update servo pins
    
    mNodeHandle->loginfo("(T'REX Controller) Shutdown!");
}
