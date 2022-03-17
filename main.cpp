//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

//=====[Defines]===============================================================

#define CODE_DIGITS                              3
#define KEYPAD_NUMBER_OF_ROWS                    4
#define KEYPAD_NUMBER_OF_COLS                    4
#define TIME_INCREMENT_MS                       10
#define DEBOUNCE_BUTTON_TIME_MS                 40
#define START_HOUR                               8
#define END_HOUR                                16

//=====[Declaration of public data types]======================================

typedef enum {
    DOOR_CLOSED,
    DOOR_UNLOCKED,
    DOOR_OPEN
} doorState_t;

typedef enum {
    MATRIX_KEYPAD_SCANNING,
    MATRIX_KEYPAD_DEBOUNCE,
    MATRIX_KEYPAD_KEY_HOLD_PRESSED
} matrixKeypadState_t;

//=====[Declaration and intitalization of public global objects]===============

DigitalOut keypadRowPins[KEYPAD_NUMBER_OF_ROWS] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn keypadColPins[KEYPAD_NUMBER_OF_COLS]  = {PB_12, PB_13, PB_15, PC_6};

DigitalIn doorHandle(BUTTON1);

DigitalOut doorUnlockedLed(LED1);
DigitalOut doorLockedLed(LED2);
DigitalOut incorrectCodeLed(LED3);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

//=====[Declaration and intitalization of public global variables]=============

int accumulatedDebounceMatrixKeypadTime = 0;

char matrixKeypadLastkeyReleased = '\0';
char matrixKeypadIndexToCharArray[] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D',
};
matrixKeypadState_t matrixKeypadState;

doorState_t doorState;

struct tm RTCTime;

time_t seconds;

char codeSequence[CODE_DIGITS] = {'1','4','7'};

//=====[Declarations (prototypes) of public functions]=========================

void uartTask();
void availableCommands();
void doorInit();
void doorUpdate();
void matrixKeypadInit();
char matrixKeypadScan();
char matrixKeypadUpdate();
void pcSerialComStringWrite( const char* str );
char pcSerialComCharRead();
void pcSerialComStringRead( char* str, int strLength );

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    doorInit();
    matrixKeypadInit();
    while (true) {
        doorUpdate();
        uartTask();
    }
}

//=====[Implementations of public functions]===================================

void uartTask()
{
    char str[100] = "";
    char receivedChar = '\0';
    struct tm rtcTime;
    char year[5] = "";
    char month[3] = "";
    char day[3] = "";
    char hour[3] = "";
    char minute[3] = "";
    char second[3] = "";
    time_t epochSeconds;
    receivedChar = pcSerialComCharRead();
    if( receivedChar !=  '\0') {
        switch (receivedChar) {

        case 's':
        case 'S':
            pcSerialComStringWrite("\r\nType four digits for the current year (YYYY): ");
            pcSerialComStringRead( year, 4);
            pcSerialComStringWrite("\r\n");
            rtcTime.tm_year = atoi(year) - 1900;

            pcSerialComStringWrite("Type two digits for the current month (01-12): ");
            pcSerialComStringRead( month, 2);
            pcSerialComStringWrite("\r\n");
            rtcTime.tm_mon  = atoi(month) - 1;

            pcSerialComStringWrite("Type two digits for the current day (01-31): ");
            pcSerialComStringRead( day, 2);
            pcSerialComStringWrite("\r\n");
            rtcTime.tm_hour = atoi(hour);

            pcSerialComStringWrite("Type two digits for the current hour (00-23): ");
            pcSerialComStringRead( hour, 2);
            pcSerialComStringWrite("\r\n");
            rtcTime.tm_hour = atoi(hour);

            pcSerialComStringWrite("Type two digits for the current minutes (00-59): ");
            pcSerialComStringRead( minute, 2);
            pcSerialComStringWrite("\r\n");
            rtcTime.tm_min  = atoi(minute);

            pcSerialComStringWrite("Type two digits for the current seconds (00-59): ");
            pcSerialComStringRead( second, 2);
            pcSerialComStringWrite("\r\n");
            rtcTime.tm_sec  = atoi(second);

            rtcTime.tm_isdst = -1;
            set_time( mktime( &rtcTime ) );
            pcSerialComStringWrite("Date and time has been set\r\n");
            break;

        case 't':
        case 'T':
            epochSeconds = time(NULL);
            sprintf ( str, "Date and Time = %s", ctime(&epochSeconds));
            pcSerialComStringWrite( str );
            pcSerialComStringWrite("\r\n");
            break;

        default:
            availableCommands();
            break;
        }
    }
}                                                                              

void availableCommands()
{
    pcSerialComStringWrite( "Available commands:\r\n" );
    pcSerialComStringWrite( "Press 's' or 'S' to set the time\r\n\r\n" );
    pcSerialComStringWrite( "Press 't' or 'T' to get the time\r\n\r\n" );
}

void doorInit()
{
    doorUnlockedLed = OFF;
    doorLockedLed = ON;
    incorrectCodeLed = OFF;
    doorState = DOOR_CLOSED;
}                                                                              

void doorUpdate()
{
    bool incorrectCode;
    char keyReleased;
    struct tm * currentTime;
    char prevkeyReleased;
    int i;

    switch( doorState ) {
    case DOOR_CLOSED:
        keyReleased = matrixKeypadUpdate();
        if ( keyReleased == 'A' ) {
            seconds = time(NULL);
            currentTime = localtime ( &seconds );

            if ( ( currentTime->tm_hour >= START_HOUR ) &&
                 ( currentTime->tm_hour <= END_HOUR ) ) {
                incorrectCode = false;
                prevkeyReleased = 'A';

                for ( i = 0; i < CODE_DIGITS; i++) {
                    while ( ( keyReleased == '\0' ) ||
                            ( keyReleased == prevkeyReleased ) ) {

                        keyReleased = matrixKeypadUpdate();
                    }
                    prevkeyReleased = keyReleased;
                    if ( keyReleased != codeSequence[i] ) {
                        incorrectCode = true;
                    }
                }

                if ( incorrectCode ) {
                    incorrectCodeLed = ON;
                    delay (1000);
                    incorrectCodeLed = OFF;
                } else {
                    doorState = DOOR_UNLOCKED;
                    doorLockedLed = OFF;
                    doorUnlockedLed = ON;
                }
            }
        }
        break;

    case DOOR_UNLOCKED:
        if ( doorHandle ) {
            doorUnlockedLed = OFF;
            doorState = DOOR_OPEN;
        }
        break;

    case DOOR_OPEN:
        if ( !doorHandle ) {
            doorLockedLed = ON;
            doorState = DOOR_CLOSED;
        }
        break;

    default:
        doorInit();
        break;
    }
}                                                                              

void matrixKeypadInit()
{
    matrixKeypadState = MATRIX_KEYPAD_SCANNING;
    int pinIndex = 0;
    for( pinIndex=0; pinIndex<KEYPAD_NUMBER_OF_COLS; pinIndex++ ) {
        (keypadColPins[pinIndex]).mode(PullUp);
    }
}                                                                              

char matrixKeypadScan()
{
    int r = 0;
    int c = 0;
    int i = 0;

    for( r=0; r<KEYPAD_NUMBER_OF_ROWS; r++ ) {

        for( i=0; i<KEYPAD_NUMBER_OF_ROWS; i++ ) {
            keypadRowPins[i] = ON;
        }

        keypadRowPins[r] = OFF;

        for( c=0; c<KEYPAD_NUMBER_OF_COLS; c++ ) {
            if( keypadColPins[c] == OFF ) {
                return matrixKeypadIndexToCharArray[r*KEYPAD_NUMBER_OF_ROWS + c];
            }
        }
    }
    return '\0';
}                                                        

char matrixKeypadUpdate()
{
    char keyDetected = '\0';
    char keyReleased = '\0';

    switch( matrixKeypadState ) {

    case MATRIX_KEYPAD_SCANNING:
        keyDetected = matrixKeypadScan();
        if( keyDetected != '\0' ) {
            matrixKeypadLastkeyReleased = keyDetected;
            accumulatedDebounceMatrixKeypadTime = 0;
            matrixKeypadState = MATRIX_KEYPAD_DEBOUNCE;
        }
        break;

    case MATRIX_KEYPAD_DEBOUNCE:
        if( accumulatedDebounceMatrixKeypadTime >=
            DEBOUNCE_BUTTON_TIME_MS ) {
            keyDetected = matrixKeypadScan();
            if( keyDetected == matrixKeypadLastkeyReleased ) {
                matrixKeypadState = MATRIX_KEYPAD_KEY_HOLD_PRESSED;
            } else {
                matrixKeypadState = MATRIX_KEYPAD_SCANNING;
            }
        }
        accumulatedDebounceMatrixKeypadTime =
            accumulatedDebounceMatrixKeypadTime + TIME_INCREMENT_MS;
        break;

    case MATRIX_KEYPAD_KEY_HOLD_PRESSED:
        keyDetected = matrixKeypadScan();
        if( keyDetected != matrixKeypadLastkeyReleased ) {
            if( keyDetected == '\0' ) {
                keyReleased = matrixKeypadLastkeyReleased;
            }
            matrixKeypadState = MATRIX_KEYPAD_SCANNING;
        }
        break;

    default:
        matrixKeypadInit();
        break;
    }
    return keyReleased;
}        

void pcSerialComStringWrite( const char* str )
{
    uartUsb.write( str, strlen(str) );
}

char pcSerialComCharRead()
{
    char receivedChar = '\0';
    if( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
    }
    return receivedChar;
}

void pcSerialComStringRead( char* str, int strLength )
{
    int strIndex;
    for ( strIndex = 0; strIndex < strLength; strIndex++) {
        uartUsb.read( &str[strIndex] , 1 );
        uartUsb.write( &str[strIndex] ,1 );
    }
    str[strLength]='\0';
}