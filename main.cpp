#include "mbed.h"
#include "rtos.h"

//Photointerrupter input pins
#define I1pin D2
#define I2pin D11
#define I3pin D12

//Incremental encoder input pins
#define CHA   D7
#define CHB   D8  

//Motor Drive output pins   //Mask in output byte
#define L1Lpin D4           //0x01
#define L1Hpin D5           //0x02
#define L2Lpin D3           //0x04
#define L2Hpin D6           //0x08
#define L3Lpin D9           //0x10
#define L3Hpin D10          //0x20

//Mapping from sequential drive states to motor phase outputs
/*
State   L1  L2  L3
0       H   -   L
1       -   H   L
2       L   H   -
3       L   -   H
4       -   L   H
5       H   L   -
6       -   -   -
7       -   -   -
*/
//Drive state to output table
const int8_t driveTable[] = {0x12,0x18,0x09,0x21,0x24,0x06,0x00,0x00};

//Mapping from interrupter inputs to sequential rotor states. 0x00 and 0x07 are not valid
const int8_t stateMap[] = {0x07,0x05,0x03,0x04,0x01,0x00,0x02,0x07};  
//const int8_t stateMap[] = {0x07,0x01,0x03,0x02,0x05,0x00,0x04,0x07}; //Alternative if phase order of input or drive is reversed

//Phase lead to make motor spin
const int8_t lead = -2;  //2 for forwards, -2 for backwards

//Status LED
DigitalOut led1(LED1);

//Photointerrupter inputs
DigitalIn I1(I1pin);
DigitalIn I2(I2pin);
DigitalIn I3(I3pin);

//Motor Drive outputs
DigitalOut L1L(L1Lpin);
DigitalOut L1H(L1Hpin);
DigitalOut L2L(L2Lpin);
DigitalOut L2H(L2Hpin);
DigitalOut L3L(L3Lpin);
DigitalOut L3H(L3Hpin);

//Set a given drive state
void motorOut(int8_t driveState){
    
    //Lookup the output byte from the drive state.
    int8_t driveOut = driveTable[driveState & 0x07];
      
    //Turn off first
    if (~driveOut & 0x01) L1L = 0;
    if (~driveOut & 0x02) L1H = 1;
    if (~driveOut & 0x04) L2L = 0;
    if (~driveOut & 0x08) L2H = 1;
    if (~driveOut & 0x10) L3L = 0;
    if (~driveOut & 0x20) L3H = 1;
    
    //Then turn on
    if (driveOut & 0x01) L1L = 1;
    if (driveOut & 0x02) L1H = 0;
    if (driveOut & 0x04) L2L = 1;
    if (driveOut & 0x08) L2H = 0;
    if (driveOut & 0x10) L3L = 1;
    if (driveOut & 0x20) L3H = 0;
    }
    
    //Convert photointerrupter inputs to a rotor state
inline int8_t readRotorState(){
    return stateMap[I1 + 2*I2 + 4*I3];
    }

//Basic synchronisation routine    
int8_t motorHome() {
    //Put the motor in drive state 0 and wait for it to stabilise
    motorOut(0);
    wait(1.0);
    
    //Get the rotor state
    return readRotorState();
}

//Task Starter
void threadStarter();
void interruptUpdateMotor();

//Task 1
void threadSetVelocity();

//Task 2
void threadSpinMotor();
void interruptSpinMotor();

/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/

Serial pc(SERIAL_TX, SERIAL_RX);

//Main
int main() {
#if 1
    int8_t orState = 0;    //Rotot offset at motor state 0
    
    //Initialise the serial port
    //Serial pc(SERIAL_TX, SERIAL_RX);
    int8_t intState = 0;
    int8_t intStateOld = 0;
    pc.printf("Hello\n\r");
    
    //Run the motor synchronisation
    orState = motorHome();
    pc.printf("Rotor origin: %x\n\r",orState);
    //orState is subtracted from future rotor state inputs to align rotor and motor states
    
    //Poll the rotor state and set the motor outputs accordingly to spin the motor
    while (1) {
        intState = readRotorState();
        if (intState != intStateOld) {
            intStateOld = intState;
            motorOut((intState-orState+lead+6)%6); //+6 to make sure the remainder is positive
            pc.printf("new rotor state = %d\n\r", intState);
        }
    }
#else
    //Start running threadStarter
    Thread thrStarter;
    //thrStarter.start(threadStarter);
    
    while (1) {
        pc.printf("Running main thread. Is motor spinning?\n\r");
        wait(1.0);
    }
#endif
}

/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/

//Run starter code with threading and interrupts
InterruptIn sI1In(I1pin);
InterruptIn sI2In(I2pin);
InterruptIn sI3In(I3pin);

//orState is subtracted from future rotor state inputs to align rotor and motor states
int8_t orState;

void threadStarter() {
    //Run the motor synchronisation
    orState = motorHome();
 
    //Attach ISR to interrupt pins
    sI1In.rise(&interruptUpdateMotor);
    sI1In.fall(&interruptUpdateMotor);
    sI2In.rise(&interruptUpdateMotor);
    sI2In.fall(&interruptUpdateMotor);
    sI3In.rise(&interruptUpdateMotor);
    sI3In.fall(&interruptUpdateMotor);
    
    while (1) {
        //wait for interrupts
    }
}

void interruptUpdateMotor(){
    int8_t intState = readRotorState();
    motorOut((intState-orState+lead+6)%6); //+6 to make sure the remainder is positive
}

/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/

//Spin motor at specified velocity (no. rotations per second)
void threadSetVelocity(){
    orState = motorHome();
    wait(1.0);
    /*
    while(1) {
        for (int i = 0; i < 6; i++) {
            motorOut((i-orState+6)%6);
            wait(1);
        }
    }
    */
    /*
    double delay = 0.3;
    while (1) {
        for (int i = 0; i < 6; i++) {
            motorOut((i-orState+6)%6);
            wait(delay);
        }
        if (delay > 0.05) {
            delay *= 0.8;
        }
    }
    */
    int rotations = 2;  //rotations per second
    double minDelay = (1.0/(6*double(rotations)));
    double delay = (minDelay > 0.3) ? minDelay : 0.3;
    Timer t;
    while (1) {
        t.start();
        printf("\n\rdelay is %f\n\r", delay);
        printf("min delay is %f\n\r", minDelay);
        for (int i = 0; i < 6; i++) {
            motorOut((i-orState+6)%6);
            wait(delay);
        }
        if (delay > minDelay && delay > 0.05) {
            delay *= 0.5;
            if (delay < minDelay) { delay = minDelay; }
        }
        t.stop();
        printf("time for one rotation: %f\n\r", t.read());
        t.reset();
    }
}


/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/

//Spin motor for a defined number of rotations
volatile int speed = 512;
volatile int count = 0;

void threadSpinMotor() {
    orState = motorHome();
    wait(1.0);
    int rotations = 50;
    int dist = 6*rotations;
    speed = 512;
    count = 0;
    int error;
    sI1In.rise(&interruptSpinMotor);
    sI1In.fall(&interruptSpinMotor);
    sI2In.rise(&interruptSpinMotor);
    sI2In.fall(&interruptSpinMotor);
    sI3In.rise(&interruptSpinMotor);
    sI3In.fall(&interruptSpinMotor);
    interruptSpinMotor();
    error = dist - count;
    while (error > 0) {
        error = dist - count;
        //speed = (error > 512) ? 512 : error;
        speed = 512;
        pc.printf("speed = %d, count = %d, error = %d\n\r", (speed>>8), count, error);
        int8_t intState = readRotorState();
        pc.printf("currentState is %d\n\r", intState);
    }
    speed = 0;
    printf("Finished Function\n\r");
    printf("Finished Function\n\r");
}

void interruptSpinMotor() {
    int8_t intState = readRotorState();
    count+= (speed >> 8);
    motorOut((intState-orState+(speed>>8)+6)%6); //+6 to make sure the remainder is positive
}



/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/
int oldState, newState;
Timer tVelocity;
double currentVelocity = 0; //in rotations per second
double newVelocity = 0; // new rotation speed set by PID

double pid(double targetVelocity, double prevVelocity, double &sumError){
    double error = targetVelocity - currentVelocity;
    sumError += error;
    double prevError = targetVelocity - prevVelocity;

    double kp = 10;
    double ki = 0;
    double kd = 0;


    double proportional = kp * error;
    double intergral = ki * sumError;
    double differential = kd * (error - prevError);

    newVelocity = proportional + integral + differential;
}

void calculateVelocity() {
    //states go from 0-6 and wrap around
    //t should be started beforehand
    t.stop();
    double time_ - t.read();
    
    int stateChange = (newState - oldState)
    if (stateChange >=0) {
        currentVelocity =  double(stateChange%6)/time_;
    }
    else {
        currentVelocity = -double(stateChange%6)/time_;
    }
}

void spinMotor() {
    //spin motor based on newVelocity
    int lead = 2;
    while (1) {
        
    }
}

void setVelocity(float velocity) {
    //spins motor based on given velocity in rotations per second
    //Declare a timer used to calcualte velocity
    tVelocity.start();
    sI1In.rise(&calculateVelocity);
    sI1In.fall(&calculateVelocity);
    sI2In.rise(&calculateVelocity);
    sI2In.fall(&calculateVelocity);
    sI3In.rise(&calculateVelocity);
    sI3In.fall(&calculateVelocity);
    Thread th;
    t.start(spinMotor);
}