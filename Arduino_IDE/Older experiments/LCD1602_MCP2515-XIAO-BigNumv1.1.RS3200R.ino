/*

*/


//SeeedStudio CAN BUS library
const int SPI_CS_PIN = 7;
const int CAN_INT_PIN = 2;
#include <SPI.h>
#include "mcp2515_can.h"
mcp2515_can CAN(SPI_CS_PIN); // Set CS pin

#define PID_ENGIN_RPM       0x0C
#define PID_COOLANY_TEMP    0x05
#define PID_INLET_TEMP      0x0F //8bit =A-40      
#define PID_ECU_VOLTAGE     0x42 //16bit =(256*A+B)/1000

#define CAN_ID_PID          0x7DF
#define SWM_GEAR_PID        0x312

//char PID_INPUT[3] = {0x05, 0x0F, 0x42};

unsigned char PID_INPUT;
int loopcount = 0;
unsigned char getPid    = 0;
unsigned char rpm = 0;

// Display section
#include <LiquidCrystal_I2C.h> 
/* You can get the LiquidCrystal_I2C library it at https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library */
#include <BigNumbers_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2); // construct LCD object
BigNumbers_I2C bigNum(&lcd); // construct BigNumbers_I2C object, passing to it the name of our LCD object


void set_mask_filt() {
    /*
        set mask, set both the mask to 0x3ff
    */
    CAN.init_Mask(0, 0, 0x7FF);
    CAN.init_Mask(1, 0, 0x7FF);

    /*
        set filter, we can receive id from 0x04 ~ 0x09
    */
    CAN.init_Filt(0, 0, 0x7E8);
    CAN.init_Filt(1, 0, 0x312);

    CAN.init_Filt(2, 0, 0x312);
    CAN.init_Filt(3, 0, 0x312);
    CAN.init_Filt(4, 0, 0x312);
    CAN.init_Filt(5, 0, 0x312);

//from receive_check example-----------------------------
//Get data from ID: 0x312
//0  0 46  2 0 0 0 

//from this OBD2_PID example------------------------------------------------------------------
//Get Data From id: 0x312
//0x0  0x0 0x46  0x5 0x0 0x0 0x0 
}
void set_mask_filtlow() {
Serial.println("Filter Low");
    /*
        set mask, set both the mask to 0x3ff
    */
    CAN.init_Mask(0, 0, 0x0);
    CAN.init_Mask(1, 0, 0x0);

    /*
        set filter, we can receive id from 0x04 ~ 0x09
    */
    CAN.init_Filt(0, 0, 0x0);
    CAN.init_Filt(1, 0, 0x0);

    CAN.init_Filt(2, 0, 0x0);
    CAN.init_Filt(3, 0, 0x0);
    CAN.init_Filt(4, 0, 0x0);
    CAN.init_Filt(5, 0, 0x0);
}

void sendPid(unsigned char __pid) {
    unsigned char tmp[8] = {0x02, 0x01, __pid, 0, 0, 0, 0, 0};
//    SERIAL_PORT_MONITOR.print("SEND PID: 0x");
//    SERIAL_PORT_MONITOR.println(__pid, HEX);
    CAN.sendMsgBuf(CAN_ID_PID, 0, 8, tmp);
}


byte g;
byte hour = 18;
byte minute = 47;
int inlettemp=23;
int coolanttemp=101;
int stuff=0;

int voltage=12;
String m, h, v, a, c;

void setup()
{

  lcd.begin(); // setup LCD rows and columns
  bigNum.begin(); // set up BigNumbers
  lcd.clear(); // clear display

    SERIAL_PORT_MONITOR.begin(115200);
//    while(!Serial){};

    while (CAN_OK != CAN.begin(CAN_500KBPS)) {             // init can bus : baudrate = 500k
        SERIAL_PORT_MONITOR.println("CAN init fail, retry...");
        delay(100);
    }
    SERIAL_PORT_MONITOR.println("CAN init ok!");
set_mask_filt(); 
}

byte x = 6;//x & y determines position of character on screen
byte y = 0;

void loop()
{

unsigned long canId;
unsigned char len = 0;
unsigned char buf[8];


h = String(hour);
m = String(minute);
v = String(voltage);
a = String(inlettemp);
c = String(coolanttemp);
//g = loopcount;

  bigNum.displayLargeNumber(g, x, y);
  lcd.setCursor(0, 0);
  lcd.print(h + ":" + m + " ");
  lcd.setCursor(10, 0);
  lcd.print("E " + a + char(223));
  lcd.setCursor(0, 1);
  lcd.print(v + "V");
  lcd.setCursor(10, 1);
  lcd.print("A " + c + char(223));

 // loop counter   
    if (loopcount == 1) {
        PID_INPUT = 0x05;
    }
    if (loopcount == 2) {
        PID_INPUT = 0x0F;
    }
    if (loopcount == 3) {
        PID_INPUT = 0x42;
        loopcount = 0;
    }
//SERIAL_PORT_MONITOR.print("Loopcount :");
//SERIAL_PORT_MONITOR.println(loopcount);
    sendPid(PID_INPUT);
    ++loopcount;



    if (CAN_MSGAVAIL == CAN.checkReceive()) {                // check if get data
        CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf

        SERIAL_PORT_MONITOR.println("\r\n------------------------------------------------------------------");
        SERIAL_PORT_MONITOR.print("Get Data From id: 0x");
        canId = CAN.getCanId();
        SERIAL_PORT_MONITOR.println(canId, HEX);
        for (int i = 0; i < len; i++) { // print the data
            SERIAL_PORT_MONITOR.print("0x");
            SERIAL_PORT_MONITOR.print(buf[i], HEX);
            SERIAL_PORT_MONITOR.print("\t");
        }
        SERIAL_PORT_MONITOR.println();
    }


//ISO 
        if (canId == 0x312 && buf[0] == 0x00) {            //&& buf[1] == 0x00
          g=buf[3];
          SERIAL_PORT_MONITOR.print("Gear: ");
          SERIAL_PORT_MONITOR.println(g, HEX);
          bigNum.displayLargeNumber(g, x, y);
        }

        if (canId == 0x7E8 && buf[2] == PID_COOLANY_TEMP) {
          coolanttemp = buf[3]-40;
          c = String(coolanttemp);
          lcd.setCursor(10, 0);
          lcd.print("E " + a + char(223));
        }
        if (canId == 0x7E8 && buf[2] == PID_INLET_TEMP) {
          inlettemp = buf[3]-40;
          a = String(inlettemp);
          lcd.setCursor(10, 1);
          lcd.print("A " + c + char(223));
        }
        if (canId == 0x7E8 && buf[2] == PID_ECU_VOLTAGE) {
          voltage = (buf[3]*256 + buf[4])/1000;
          v = String(voltage);
          lcd.setCursor(0, 1);
          lcd.print(v + "V");
        }
        if (canId == 0x7E8 && buf[2] == PID_ENGIN_RPM) {
        rpm = (buf[3]*256 + buf[4])/4;
        }
        //Gear

delay(500);

h = String(hour);
m = String(minute);
lcd.setCursor(0, 0);
lcd.print(h + ":" + m + " ");



}
