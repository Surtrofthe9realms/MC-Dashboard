/*

*/
//SeeedStudio CAN BUS library
const int SPI_CS_PIN = 7;
const int CAN_INT_PIN = 2;
#include <SPI.h>
#include <mcp2515_can.h>
mcp2515_can CAN(SPI_CS_PIN); // Set CS pin

#define PID_ENGIN_RPM       0x0C
#define PID_COOLANY_TEMP    0x05
#define PID_INLET_TEMP      0x0F //8bit =A-40      
#define PID_ECU_VOLTAGE     0x42 //16bit =(256*A+B)/1000

#define CAN_ID_PID          0x7DF
#define SWM_GEAR_PID        0x312

//char PID_INPUT[3] = {0x05, 0x0F, 0x42};

unsigned char PID_INPUT;
int loopcount = 1;
unsigned char getPid    = 0;


// Display section
#include <LiquidCrystal_I2C.h> 
/* You can get the LiquidCrystal_I2C library it at https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library */
#include <BigNumbers_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2); // construct LCD object
BigNumbers_I2C bigNum(&lcd); // construct BigNumbers_I2C object, passing to it the name of our LCD object

void sendPid(unsigned char __pid) {
    unsigned char tmp[8] = {0x02, 0x01, __pid, 0, 0, 0, 0, 0};
    //SERIAL_PORT_MONITOR.print("SEND PID: 0x");
    //SERIAL_PORT_MONITOR.println(__pid, HEX);
    CAN.sendMsgBuf(CAN_ID_PID, 0, 8, tmp);
}


byte g = 8;
byte lastg = 8;
int inlettemp;
int lastinlettemp;
int coolanttemp;
int lastcoolanttemp;
int rpm;
int lastrpm;

int stuff=0;
String  r, it, ct;

void setup()
{
  byte x = 13;//x & y determines position of character on screen
  byte y = 0;//   bigNum.displayLargeNumber(g, x, y);
  lcd.begin(); // setup LCD rows and columns
  bigNum.begin(); // set up BigNumbers
  lcd.clear(); // clear display
  //SERIAL_PORT_MONITOR.begin(9600);
  //SERIAL_PORT_MONITOR.println("Wake up....");
lcd.setCursor(0, 0);
lcd.print("XIAO SD 1.4");

  bigNum.displayLargeNumber(g, x, y);
  //  lcd.setCursor(0, 1);
  //  lcd.print("ET --" );
  //  lcd.setCursor(7, 1);
  //  lcd.print("IT --");
  //  lcd.setCursor(0, 0);
  //  lcd.print("RPM --");

    while (CAN_OK != CAN.begin(CAN_500KBPS)) {             // init can bus : baudrate = 500k
        //SERIAL_PORT_MONITOR.println("CAN init fail, retry...");
        delay(100);
//        lcd.setCursor(0, 1);
//        lcd.print("CAN init fail, retry...");
    }
    //SERIAL_PORT_MONITOR.println("CAN init ok!");
//lcd.setCursor(0, 1);
//lcd.print("CAN init ok!");

  // Set up filter and mask registers
  unsigned long mask0 = 0x7FF;    // Mask all bits for filter 0
  unsigned long mask1 = 0x7FF;    // Mask all bits for filter 1
  unsigned long filter0 = 0x7E8;  // Standard CAN id 0x7E8
  unsigned long filter1 = 0x312;  // Standard CAN id 0x312
  // // Configure filter and mask registers
  CAN.init_Mask(0, 0, mask0);    // Mask all bits for filter 0
  CAN.init_Mask(1, 0, mask1);    // Mask all bits for filter 1
  CAN.init_Filt(0, 0, filter0);  // Allow standard CAN id 0x7E8 for filter 0
  CAN.init_Filt(1, 0, filter1);  // Allow standard CAN id 0x312 for filter 1
  CAN.init_Filt(2, 0, filter1);  // Allow standard CAN id 0x312 for filter 2
  CAN.init_Filt(3, 0, filter1);  // Allow standard CAN id 0x312 for filter 3
  CAN.init_Filt(4, 0, filter1);  // Allow standard CAN id 0x312 for filter 4
  CAN.init_Filt(5, 0, filter1);  // Allow standard CAN id 0x312 for filter 5

}


byte x = 13;//x & y determines position of character on screen
byte y = 0;
int bar;

void loop()
{

unsigned long canId;
unsigned char len = 0;
unsigned char buf[8];

if (CAN_MSGAVAIL == CAN.checkReceive()) {
  CAN.readMsgBuf(&len, buf); 
  canId = CAN.getCanId();
}

// r = String(rpm);
// it = String(inlettemp);
// ct = String(coolanttemp);
// //g = loopcount;

 // loop counter   
    if (loopcount == 1) {
        PID_INPUT = 0x05;
    }
    if (loopcount == 2) {
        PID_INPUT = 0x0F;
    }
    if (loopcount == 3) {
        PID_INPUT = 0x0C; //RPM
        loopcount = 0;
    }

 
//SERIAL_PORT_MONITOR.print("Loopcount :");
//SERIAL_PORT_MONITOR.println(loopcount);

sendPid(PID_INPUT);

int loopcount2 = 1;
    while (canId != 0x7E8 and loopcount2 < 20)
    {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
      CAN.readMsgBuf(&len, buf); 
      canId = CAN.getCanId();
    }
    if (canId == 0x7E8 && buf[2] == PID_COOLANY_TEMP) {
      coolanttemp = buf[3]-40;
      if ( coolanttemp != lastcoolanttemp) {
        ct = String(coolanttemp);
        unsigned int lsl = ct.length();
        lcd.setCursor(0, 1);
        lcd.print("CT ");
        lcd.setCursor(5-lsl, 1);
        lcd.print(ct + char(223) + " ");
        lastcoolanttemp = coolanttemp;
      }
//          //SERIAL_PORT_MONITOR.print(lsl , (HEX));
//          //SERIAL_PORT_MONITOR.println(" " + ct);
    }
    if (canId == 0x7E8 && buf[2] == PID_INLET_TEMP) {
      inlettemp = buf[3]-40;
      if ( inlettemp != lastinlettemp) {
        it = String(inlettemp);
        unsigned int lsl = it.length();
        lcd.setCursor(7, 1);
        lcd.print("IT ");
        lcd.setCursor(12-lsl, 1);
        lcd.print(it + char(223));
        lastinlettemp = inlettemp;
      }
    }
    if (canId == 0x7E8 && buf[2] == PID_ENGIN_RPM) {
     rpm = (buf[3]*256 + buf[4])/4;
//        for (int i = 0; i < (r / 500); i++) {
//       lcd.write(255);
//        }
      if ( rpm != lastrpm) {
        rpm = rpm/100;
        r = String(rpm);
        rpm = rpm -10;
        lcd.setCursor(0, 0);
//       lcd.print("RPM " + r + " ");
        lcd.print( r); 
        for (int i = 0; i < (rpm / 5); i++) {
        lcd.write(255);
        bar = 10 - i;
        }
        for (int i = 0; i < bar; i++) {
        lcd.write(32);
        }

        lastrpm = rpm;
      }
    }
    ++loopcount2;
  }


    while (canId != 0x312)
    {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
      CAN.readMsgBuf(&len, buf); 
      canId = CAN.getCanId();
    }
      //Gear
      if (canId == 0x312 && buf[0] == 0x00) {            //&& buf[1] == 0x00
        g=buf[3];
        if (g != lastg) {
          bigNum.displayLargeNumber(g, x, y);
          lastg = g;
        }
      }
    }

delay(250);

   ++loopcount;

}
