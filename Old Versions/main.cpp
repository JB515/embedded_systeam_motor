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
volatile int8_t lead = -2;  //2 for forwards, -2 for backwards

//Status LED
DigitalOut led1(LED1);

//Photointerrupter inputs
DigitalIn I1(I1pin);
DigitalIn I2(I2pin);
DigitalIn I3(I3pin);

//encoder input pins
DigitalIn IncA(CHA);
DigitalIn IncB(CHB);

//Motor Drive outputs
PwmOut L1L(L1Lpin);
PwmOut L1H(L1Hpin);
PwmOut L2L(L2Lpin);
PwmOut L2H(L2Hpin);
PwmOut L3L(L3Lpin);
PwmOut L3H(L3Hpin);

//Set a given drive state
void motorOut(int8_t driveState, double delta=1) {
    
    //Lookup the output byte from the drive state.
    int8_t driveOut = driveTable[driveState & 0x07];
    
    /*
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
    */

    
    //Turn off first
    if (~driveOut & 0x01) L1L = 0;
    if (~driveOut & 0x02) L1H = delta;
    if (~driveOut & 0x04) L2L = 0;
    if (~driveOut & 0x08) L2H = delta;
    if (~driveOut & 0x10) L3L = 0;
    if (~driveOut & 0x20) L3H = delta;
    
    //Then turn on
    if (driveOut & 0x01) L1L = delta;
    if (driveOut & 0x02) L1H = 0;
    if (driveOut & 0x04) L2L = delta;
    if (driveOut & 0x08) L2H = 0;
    if (driveOut & 0x10) L3L = delta;
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
    wait(2.0);
    
    //Get the rotor state
    return readRotorState();
}

/////////////////////////////////COMMAND LINE INTERACE//////////////////////////////////////////

struct State {
    int rotate;     //1 if R
    int velocity;   //1 if V
    int state;      //different states of DFA (1 = R, 2 = V, 3 = RV)
    float rotVal;
    float velVal;
};

typedef struct State* State_h;

void stateInit(State_h s) {
    s->rotate = 0;
    s->velocity = 0;
    s->state = 0;
    s->rotVal = 0;
    s->velVal = 0;
}

int parseDigit(char c) {
    return (
            c == '0' || c == '1' || c == '2' || c == '3' || c == '4'
             || c == '5' || c == '6' || c == '7' || c == '8' || c == '9'
            );
}

//should only be called if parseDigit is successful
int readDigit(char c) {
    char* digitArray = "0123456789";
    for (int i=0; i < 10; i++) {
        if (digitArray[i] == c) {
            return i;
        }
    }
    return -1;
}

int parseChar(char c, char input) {
    return (input==c);
}

///returns the index of the last digit of the number and sets result
int parseNumber(char* s, int start, float* result) {
    int foundDecimal = 0;
    int foundMinus = 0;
    int i = start;
    while (i < 16 && (parseDigit(s[i]) || parseChar('.', s[i]) || parseChar('-', s[i]))) {
        if (parseChar('.', s[i]) && (i == start || foundDecimal)) {
            i = 0;
            break;
        }
        else if (parseChar('.', s[i])){
            foundDecimal = 1;
        }
        else if (parseChar('-', s[i]) && (i != start || foundMinus)) {
            i = 0;
            break;
        }
        else if (parseChar('-', s[i])){
            foundMinus = 1;
        }
        i++;
    }
    //if we managed to parse one of more digits
    if (i > start) {
        //take the characters between and including start and i
        char num[17] = "0000000000000000";
        for (int j = start; j < i+1; j++) {
            num[j-start] = s[j];
        }
        *result = atof(num);
        return i;
    }
    else {
        //falied to parse any number
        return -1;
    }
}

void threadReadInput();

///////////////////////////////COMMAND LINE INTERACE END////////////////////////////////////////

/////////////////////////////////GLOBAL VARIABLES/////////////////////////////////////////////
volatile double delta = 1.0;
Timer t_calcVel;
volatile int8_t intState = 0;
volatile int8_t intStateOld = 0;
volatile double currentTime = 0;
volatile double currentVelocity = 0;
volatile double targetVelocity = 15;
volatile double maxVelocity = 0;
volatile double currentNumOfRotationsLeft = 0.0;    //not used now
volatile double currentNumOfRotations = 0.0;
volatile double numOfRotations = 10.0;
volatile double oldError = 0;
Serial pc(SERIAL_TX, SERIAL_RX);
//Run starter code with threading and interrupts
InterruptIn sI1In(I1pin);
InterruptIn sI2In(I2pin);
InterruptIn sI3In(I3pin);
InterruptIn chAIn(I2pin);
InterruptIn chBIn(I3pin);

Timer t_recordMaxVel;


/////////////////////////////////FUNCTION DECLARATIONS//////////////////////////////////////////

//Task Starter
void threadStarter();
void interruptUpdateMotor();

//Task velocity
//void recordMaxVelocity();
//void calculateMaxVelocity();
void setVelocity();
void calculateVelocity();
Thread thrSetVelocity;
void calculateNumRotationsVelocity();

//Task position
void calculateNumRotationsLeft();
void setRotation();
void setRotationVelocity();
void pushMotor();



//orState is subtracted from future rotor state inputs to align rotor and motor states
int8_t orState = 0;

/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/



//Main
int main() {
#if 0
    
    //Initialise the serial port
    //Serial pc(SERIAL_TX, SERIAL_RX);
    //int8_t intState = 0;
    //int8_t intStateOld = 0;
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
            //pc.printf("new rotor state = %d\n\r", intState);
        }
    }
#else
    //thrSetVelocity.start(setVelocity);
    //Thread thrReadInput;
    //thrReadInput.start(threadReadInput);
    //recordMaxVelocity();
    //setVelocity();
    //setRotation();
    //setRotationVelocity();
    threadReadInput();
    while (1) {
        Thread::wait(10000);
    }
#endif
}

/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/
/*
void recordMaxVelocity() {
    //Initialise the serial port
    //Serial pc(SERIAL_TX, SERIAL_RX);
    //int8_t intState = 0;
    //int8_t intStateOld = 0;
    pc.printf("Hello: measuring max speed:\n\r");
    
    //Run the motor synchronisation
    orState = motorHome();
    pc.printf("Rotor origin: %x\n\r",orState);
    //orState is subtracted from future rotor state inputs to align rotor and motor states
    maxVelocity = 0;
    t_calcVel.start();
    t_recordMaxVel.start();
    //Poll the rotor state and set the motor outputs accordingly to spin the motor
    while (1) {
        intState = readRotorState();
        if (intState != intStateOld) {
            intStateOld = intState;
            motorOut((intState-orState+lead+6)%6); //+6 to make sure the remainder is positive
            //pc.printf("new rotor state = %d\n\r", intState);
            calculateMaxVelocity();
        }
        double timePassed = t_recordMaxVel.read();
        if (timePassed > 10.0) {
            t_recordMaxVel.stop();
            t_recordMaxVel.reset();
            break;
        }
    }
    if (maxVelocity <= 0) {
        recordMaxVelocity();
        return;
    }
    pc.printf("\n\rDone: max velocity = %f", maxVelocity);
}

void calculateMaxVelocity() {
    //states go from 0-6 and wrap around
    //t_calcVel should be started beforehand
    intState = readRotorState();
    if (intState == 0) {
        t_calcVel.stop();
        double time_ = t_calcVel.read();
        if (currentTime != time_) {
            currentVelocity = 1.0/time_;
            if (currentVelocity > maxVelocity) {
                maxVelocity = currentVelocity;
            }
            //pc.printf("currentVelocity = %f, targetVelocity = %f, delta = %F\n\r",currentVelocity, targetVelocity, delta);
            pc.printf(" %f ",currentVelocity);
            currentTime = time_;
        }
        t_calcVel.reset();
        t_calcVel.start();
    }
}
*/

void threadStarter() {
    //Run the motor synchronisation
    pc.printf("Starting thread...\n\r");
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

//////////////////////////////////////////////////////////////////////////////////////////
/*
void setVelocity (){
    int dutyCycle = 1;
    orState = motorHome();
    int currentState;
    while (1){
        currentState = readRotorState ();
        motorOut (0x7);
        wait ( (1 - dutyCycle) * 0.01);
        motorOut(currentState - orState +2); 
        wait ( (dutyCycle) * 0.01); 

    }
    
 }
 */
 ////////////////////////////////////////////////////////////////////////////////////////
 
void threadReadInput() {
    
    while (1) {
        pc.printf("Please type some input (note: input does not show and reset is required after each command):\n\r");
        char input[16];
        State_h s = (struct State*)malloc(sizeof(struct State));
        stateInit(s);
        //scan input
        pc.scanf("%s", &input);
        //pcprintf("input = %s\n", input);
        //parse input
        int i = 0;
        while (s->state < 2 && input[i] != '\0') {
            switch (input[i]) {
                case 'R': case 'r':
                    if (s->state == 0) {
                        float val = 0.0;
                        int newPos = parseNumber(input, i+1, &val);
                        if (newPos != -1) {
                            //printf("rotVal = %f\n", val);
                            s->state = 1;
                            s->rotate = 1;
                            s->rotVal = val;
                            i = newPos-1;
                        }
                    }
                    break;
                case 'V': case 'v':
                    if (s->state == 0 || s->state == 1) {
                        float val = 0.0;
                        int newPos = parseNumber(input, i+1, &val);
                        if (newPos != -1) {
                            //printf("velVal = %f\n", val);
                            s->state = 2;
                            s->velocity = 1;
                            s->velVal = val;
                            i = newPos-1;
                        }
                    }
            }

            i++;
        }
        //call threads here
        if (s->rotate && s->velocity) {
            pc.printf("Calling rotate and velocity function with R=%f, V=%f...\n\r", s->rotVal, s->velVal);
            //insert here
            maxVelocity = s->velVal;
            if (maxVelocity > 5) {
                maxVelocity = 5;
            }
            numOfRotations = s->rotVal;
            setRotationVelocity();
            
        }
        else if (s->rotate) {
            pc.printf("Calling just rotate function with R=%f...\n\r", s->rotVal);
            maxVelocity = 5.0;
            numOfRotations = s->rotVal;
            setRotationVelocity();
        }
        else if (s->velocity) {
            pc.printf("Calling just velocity function with V=%f...\n\r", s->velVal);
            targetVelocity = s->velVal;
            setVelocity();
        }
        free(s);
        Thread::wait(5000);
    }
}


volatile bool velDecreasing = false;
Timer t_motorPeriod;
volatile double velErrorDeltaSum = 0;
volatile double posError = -1;


void setVelocity() {
    //set target velocity
    //targetVelocity = -15.0;
    lead = 2;
    
    if (targetVelocity < 0) {
        targetVelocity = - targetVelocity;                                             
        lead = -2;
    }
    
    delta = 1;
    t_calcVel.start();
    t_motorPeriod.start();
    
    pc.printf("Hello\n\r");
    
    //Run the motor synchronisation
    orState = motorHome();
    pc.printf("Rotor origin: %x\n\r",orState);
    //orState is subtracted from future rotor state inputs to align rotor and motor states
    
    //Poll the rotor state and set the motor outputs accordingly to spin the motor
    while (1) {
        intState = readRotorState();
        //delta = 1.0;
        if (intState != intStateOld) {
            intStateOld = intState;       
            motorOut((intState-orState+lead+6)%6, delta); //+6 to make sure the remainder is positive
            calculateVelocity();
        }
        else {
            currentVelocity = 0.0;
        }
        //Thread::wait(1);
    }
}                                                            

void calculateVelocity() {
    //states go from 0-6 and wrap around
    //t_calcVel should be started beforehand
    intState = readRotorState();
    if (intState == orState) {
        t_calcVel.stop();
        double time_ = t_calcVel.read();
        if (currentTime != time_) {
            currentVelocity = 1.0/time_;
            //measure period
            
            //pc.printf("currentVelocity = %f, targetVelocity = %f, delta = %F\n\r",currentVelocity, targetVelocity, delta);
            //pc.printf(" %f ",currentVelocity);
            currentTime = time_;
            //set delta using PID
            double error = targetVelocity - currentVelocity;
            //double k_p = 0.01;
            double k_p = 0.5;
            double k_i = 0;
            double k_d = 1.2;
            double errorDelta = (k_p*error)/10.0;
            //delta += errorDelta;
            double errorDeltaChange = (k_d*((error - oldError))/time_)/10.0;
            velErrorDeltaSum += error/10.0;
            delta += errorDelta + (k_i*velErrorDeltaSum) + errorDeltaChange;
            
            //delta += errorDelta;
            //delta = 0.0001;
            /*
             if (error < 0) {
                if (!velDecreasing) {
                    t_motorPeriod.stop();
                    pc.printf("\n\rperiod = %f\n\r", t_motorPeriod.read());
                    t_motorPeriod.reset();
                    t_motorPeriod.start();
                    velDecreasing = true;
                }
            }
            else {
                velDecreasing = false;
            }
            */
            //delta = delta + errorDelta;
            delta = (delta > 1.0) ? 1.0 : delta;
            delta = (delta < 0.0) ? 0.0 : delta;
            pc.printf(" %f \n\r",currentVelocity);
        }
        t_calcVel.reset();
        t_calcVel.start();
    }
}

bool rotatedHalf = false;

void calculateNumRotationsLeft() {
    //read currentState
    //if intState == orState
    //then one rotation has been made
    if (intState >= 3 && !rotatedHalf) {
        rotatedHalf = true;
    }
    else if(intState == 0 && rotatedHalf) {
        rotatedHalf = 0;
        currentNumOfRotationsLeft -= 1.0;
        if (currentNumOfRotationsLeft < 100) {
            if (currentNumOfRotationsLeft > 43)
            {
                delta = 0.9216;
            }
            else {
                double maxCount = (numOfRotations > 43) ? 43 : numOfRotations;
                //delta = (currentNumOfRotationsLeft*currentNumOfRotationsLeft)/(numOfRotations*numOfRotations);
                delta = (currentNumOfRotationsLeft*currentNumOfRotationsLeft)/(maxCount*maxCount);
                delta = (delta > 1.0) ? 1.0 : delta;
                delta = (delta < 0.0) ? 0.0 : delta;
            }
        }
        else {
            delta = 1;
        }
        pc.printf("currentNumOfRotationsleft = %f, delta = %f\n\r", currentNumOfRotationsLeft, delta);
    }
}


 // Only works with full batteries
void setRotation() {
    delta = 1;
    rotatedHalf = false;
    numOfRotations = 20.0;
    lead = 2;
    if (numOfRotations < 0) {
        numOfRotations = -numOfRotations;
        lead = -2;
    }
    currentNumOfRotationsLeft = numOfRotations;
    //posError = floor(numOfRotations*177);
    pc.printf("Hello\n\r");
    
    //Run the motor synchronisation
    orState = motorHome();
    pc.printf("Rotor origin: %x\n\r",orState);
    while (currentNumOfRotationsLeft > 0) {
        intState = readRotorState();
        if (intState != intStateOld) {
            intStateOld = intState;          
            motorOut((intState-orState+lead+6)%6, delta); //+6 to make sure the remainder is positive
            calculateNumRotationsLeft();
        }
    }
    while (1) {
        intState = readRotorState();
        if (intState != intStateOld) {
            intStateOld = intState;    
            calculateNumRotationsLeft();
        }
    }
}


void threadSetVelocity() {
    //set target velocity
    thrSetVelocity.set_priority(osPriorityHigh);
    targetVelocity = 0.0;
    delta = 1;
    t_calcVel.start();
    t_motorPeriod.start();
    
    pc.printf("Hello\n\r");
    
    //Run the motor synchronisation
    orState = motorHome();
    pc.printf("Rotor origin: %x\n\r",orState);
    //orState is subtracted from future rotor state inputs to align rotor and motor states
    
    //Poll the rotor state and set the motor outputs accordingly to spin the motor
    while (1) {
        intState = readRotorState();
        if (targetVelocity < 0) {
            targetVelocity = - targetVelocity;
            lead = -2;
        }
        else {
            lead = 2;
        }
        //delta = 1.0;
        if (intState != intStateOld) {
            intStateOld = intState;
            motorOut((intState-orState+lead+6)%6, delta); //+6 to make sure the remainder is positive
            calculateVelocity();
            calculateNumRotationsVelocity();
        }
        else {
            currentVelocity = 0.0;
        }
        Thread::wait(1);
    }
}

volatile double velErrorVelocitySum = 0;
Ticker tick_push;

void calculateNumRotationsVelocity() {
    if (intState >= 3 && !rotatedHalf) {
        rotatedHalf = true;
    }
    else if(intState == 0 && rotatedHalf) {
        rotatedHalf = false;
        if (lead > 0) {
            currentNumOfRotations += 1.0;
        }
        else {
            currentNumOfRotations -= 1.0;
        }
        double error = numOfRotations - currentNumOfRotations;
        double k_p = 2;
        double k_i = 0;
        double k_d = 0;
        double errorVelocity = (k_p*error)/4.0;
        //double errorVelocityChange = (k_d*((error - oldError))/time_)/10.0;
        velErrorVelocitySum += error;
        targetVelocity = errorVelocity;
        //targetVelocity += errorVelocity + (k_i*velErrorVelocitySum) + errorVelocityChange;
        targetVelocity = (targetVelocity > maxVelocity) ? maxVelocity : targetVelocity;
        targetVelocity = (targetVelocity < -maxVelocity) ? -maxVelocity : targetVelocity;
        printf(" num so far = %f, target velocity = %f \n\r", currentNumOfRotations, targetVelocity);
        tick_push.detach();
        tick_push.attach(&pushMotor, 10.0);
    }
}

void setRotationVelocity() {
    //maxVelocity = 5.0;
    rotatedHalf = false;
    tick_push.attach(&pushMotor, 10.0);
    //numOfRotations = 100.0;
    if (numOfRotations < 0) {
        numOfRotations = -numOfRotations;
    }
    currentNumOfRotations = 0;
    pc.printf("Hello\n\r");
    
    //Run the motor synchronisation
    orState = motorHome();
    pc.printf("Rotor origin: %x\n\r",orState);
    thrSetVelocity.start(threadSetVelocity);
    while (1) {
        //printf("running this");
        Thread::wait(100);
        if (targetVelocity < 0) {
            thrSetVelocity.terminate();
            break;
        }
    }
}

void pushMotor() {
    numOfRotations = currentNumOfRotations;
    currentNumOfRotations = 0;
    tick_push.detach();
    tick_push.attach(&pushMotor, 10.0);
}
