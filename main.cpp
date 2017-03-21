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
PwmOut L1L(L1Lpin);
DigitalOut L1H(L1Hpin);
PwmOut L2L(L2Lpin);
DigitalOut L2H(L2Hpin);
PwmOut L3L(L3Lpin);
DigitalOut L3H(L3Hpin);

//Set a given drive state
void motorOut(int8_t driveState, double delta=1.0) {
    
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
    if (~driveOut & 0x02) L1H = 1;
    if (~driveOut & 0x04) L2L = 0;
    if (~driveOut & 0x08) L2H = 1;
    if (~driveOut & 0x10) L3L = 0;
    if (~driveOut & 0x20) L3H = 1;
    
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
    wait(1.0);
    
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


//Task Starter
void threadStarter();
void interruptUpdateMotor();

//Task velocity
void setVelocity();
void calculateVelocity();


/**********************************************************************************************
***********************************************************************************************
**********************************************************************************************/

Serial pc(SERIAL_TX, SERIAL_RX);

//Main
int main() {
#if 0
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
    setVelocity();
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
        pc.printf("Please type some input:\n\r");
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
        }
        else if (s->rotate) {
            pc.printf("Calling just rotate function with R=%f...\n\r", s->rotVal);
        }
        else if (s->velocity) {
            pc.printf("Calling just velocity function with V=%f...\n\r", s->velVal);
        }
        free(s);
    }
}

double delta = 1.0;
Timer t_calcVel;
int8_t intState = 0;
int8_t intStateOld = 0;
double currentTime = 0;

void setVelocity() {
    
    t_calcVel.start();
    
    //Initialise the serial port
    //Serial pc(SERIAL_TX, SERIAL_RX);
    
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
            motorOut((intState-orState+lead+6)%6, delta); //+6 to make sure the remainder is positive
            //pc.printf("new rotor state = %d\n\r", intState);
            calculateVelocity();
        }
    }
}

void calculateVelocity() {
    //states go from 0-6 and wrap around
    //t_calcVel should be started beforehand
    intState = readRotorState();
    if (intState == 0) {
        t_calcVel.stop();
        double time_ = t_calcVel.read();
        if (currentTime != time_) {
            pc.printf("%f\n\r",1.0/time_);
            currentTime = time_;
        }
        t_calcVel.reset();
        t_calcVel.start();
    }
}
