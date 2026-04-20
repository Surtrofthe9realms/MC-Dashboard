// my libraries
#include <Wire.h>
#include <LiquidCrystal.h>
#include <driver/twai.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <esp_sleep.h>
#include <esp32/rtc.h>
#include "LCDBigNumbers.hpp"

// FreeRTOS includes (ARTOS is API compatible)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//Project Version
float version = 4.82;

//lcd screen pins defined
#define LCD_RS 13
#define LCD_EN 12
#define LCD_D4 14
#define LCD_D5 7
#define LCD_D6 5
#define LCD_D7 3

//lcd big numbers
#define USE_PARALLEL_1602_LCD
#define USE_SERIAL_1602_LCD

//button pins defined
#define BUTTON1_PIN 18
#define BUTTON2_PIN 2

//ACC pin defined, used to control if and when deep sleep happens
#define ACC_PIN 1

//debug pin defined, used to activate debug program
#define DEBUG_PIN 8

//pin for shutdown of LCD screen and CANbus unit
#define POWER_PIN 38

//pins used to connect to CANbus unit
#define CAN_TX_GPIO GPIO_NUM_21
#define CAN_RX_GPIO GPIO_NUM_17

//values for the CANbus unit to attribute for information and the variables for them.
#define CAN_ID_RPM      0x100
#define CAN_ID_GEAR     0x312
#define CAN_ID_COOLANT  0x05
#define CAN_ID_VOLTAGE  0x103
#define PID_ENGINE_RPM  0x0C

int can_rpm = 0;
int can_gear = 0;
int can_coolant = 0;
int can_voltage = 0;
int obd2_rpm = 0;

//can IDs for OBD2
#define OBD2_REQUEST_ID 0x7DF
#define OBD2_RESPONSE_ID 0x7E8
#define PID_COOLANT_TEMP 0x05
#define PID_CONTROL_MODULE_VOLTAGE 0x42

int obd2_coolant_temp = -100;
float obd2_voltage = 0.0f;

unsigned long lastOBD2RequestTime = 0;
const unsigned long obd2RequestInterval = 1000;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long displayUpdateInterval = 1000;
volatile uint8_t lastRequestedOBD2PID = 0x00;

//lcd array
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

LCDBigNumbers bigNums(&lcd, BIG_NUMBERS_FONT_3_COLUMN_2_ROWS_VARIANT_2);

//debounce logic variables
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
const unsigned long debounceDelay = 100;
const unsigned long longPressDelay = 500;

//RTC data attribute variables
RTC_DATA_ATTR int hour = 0;
RTC_DATA_ATTR int minute = 0;
RTC_DATA_ATTR int cHour = 0;
RTC_DATA_ATTR int cMinute = 0;
RTC_DATA_ATTR unsigned long lastClockUpdate = 0;
RTC_DATA_ATTR int lastDisplayHour = -1;
RTC_DATA_ATTR int lastDisplayMinute = -1;

//button logic variables
bool button1State = HIGH;
bool button2State = HIGH;
bool lastButton1State = HIGH;
bool lastButton2State = HIGH;
bool button1Pressed = false;
bool button2Pressed = false;

//long press logic variables
const unsigned long interval = 350;
unsigned long button1Held = 0;
unsigned long button2Held = 0;
int button1Count = 0;
int button2Count = 0;
int heartBeat = 0;

//temp sensor pin and variables
#define DS18B20_PIN 36
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

float lastTempC = 0.0;
int64_t lastTempReadTime = 0;
const int64_t tempReadInterval = 15000000LL;

const float minDisplayTemp = 0.0;
const float maxDisplayTemp = 500.0;

//last displayed values
int lastDisplayRPM = -1;
int lastDisplayGear = -1;
int lastDisplayCoolant = -1;
bool firstDisplay = true;

//LCD CAN clear variables
unsigned long lastCANGearTime = 0;
unsigned long lastCANVoltageTime = 0;
unsigned long lastCANCoolantTempTime = 0;
unsigned long now = 0;
const unsigned long canTimeout = 2000;
volatile bool can_connected = false;
volatile unsigned long lastOBD2ResponseTime = 0;

// Forward declarations
void displayLcdScreen();
void updateClockFromRtc();
void retrieveCANData();
void requestOBD2CoolantTemp();
bool receiveOBD2Response();
void debug();
void enterDeepSleepOnAcc();
void requestOBD2Voltage();
void receiveOBD2Voltage();

// RTOS Tasks

void OBD2RequestTask(void* pvParameters) {
    while (1) {
        // Request Coolant Temp
        lastRequestedOBD2PID = PID_COOLANT_TEMP;
        twai_message_t req1 = { 0 };
        req1.identifier = OBD2_REQUEST_ID;
        req1.flags = TWAI_MSG_FLAG_NONE;
        req1.data_length_code = 8;
        req1.data[0] = 0x02;
        req1.data[1] = 0x01;
        req1.data[2] = PID_COOLANT_TEMP;
        for (int i = 3; i < 8; i++) req1.data[i] = 0x00;
        twai_transmit(&req1, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay

        // Request RPM
        lastRequestedOBD2PID = PID_ENGINE_RPM;
        twai_message_t req2 = { 0 };
        req2.identifier = OBD2_REQUEST_ID;
        req2.flags = TWAI_MSG_FLAG_NONE;
        req2.data_length_code = 8;
        req2.data[0] = 0x02;
        req2.data[1] = 0x01;
        req2.data[2] = PID_ENGINE_RPM;
        for (int i = 3; i < 8; i++) req2.data[i] = 0x00;
        twai_transmit(&req2, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay

        // Add more requests here if needed

        vTaskDelay(pdMS_TO_TICKS(800)); // Wait before next cycle (total ~1s)
    }
}

void ButtonTask(void* pvParameters) {
    while (1) {
        bool reading1 = digitalRead(BUTTON1_PIN);
        bool reading2 = digitalRead(BUTTON2_PIN);
        unsigned long currentMillis = millis();

        // Debounce button 1
        if (reading1 != lastButton1State) {
            lastDebounceTime1 = currentMillis;
        }
        if ((currentMillis - lastDebounceTime1) > debounceDelay) {
            if (reading1 != button1State) {
                button1State = reading1;
                if (button1State == LOW && !button1Pressed) {
                    button1Pressed = true;
                    button1Held = currentMillis;
                    Serial.println("Button 1 Pressed!");
                }
                if (button1State == HIGH && button1Pressed) {
                    button1Pressed = false;
                    cHour++;
                    if (cHour > 23) cHour = 0;
                    lastClockUpdate = esp_timer_get_time();
                    Serial.println("Button 1 Released!");
                }
            }
        }
        if ((currentMillis - lastDebounceTime1) > longPressDelay) {
            if (button1Pressed && button1State == LOW) {
                if (currentMillis - button1Held >= debounceDelay) {
                    cHour++;
                    if (cHour > 23) cHour = 0;
                    lastClockUpdate = esp_timer_get_time();
                    button1Held = currentMillis;
                }
            }
        }

        // Debounce button 2
        if (reading2 != lastButton2State) {
            lastDebounceTime2 = currentMillis;
        }
        if ((currentMillis - lastDebounceTime2) > debounceDelay) {
            if (reading2 != button2State) {
                button2State = reading2;
                if (button2State == LOW && !button2Pressed) {
                    button2Pressed = true;
                    button2Held = currentMillis;
                    Serial.println("Button 2 Pressed!");
                }
                if (button2State == HIGH && button2Pressed) {
                    button2Pressed = false;
                    cMinute++;
                    if (cMinute > 59) cMinute = 0;
                    lastClockUpdate = esp_timer_get_time();
                    Serial.println("Button 2 Released!");
                }
            }
        }
        if ((currentMillis - lastDebounceTime2) > longPressDelay) {
            if (button2Pressed && button2State == LOW) {
                if (currentMillis - button2Held >= debounceDelay) {
                    cMinute++;
                    if (cMinute > 59) cMinute = 0;
                    lastClockUpdate = esp_timer_get_time();
                    button2Held = currentMillis;
                }
            }
        }

        lastButton1State = reading1;
        lastButton2State = reading2;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void DisplayTask(void* pvParameters) {
    while (1) {
        displayLcdScreen();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void ClockTask(void* pvParameters) {
    while (1) {
        updateClockFromRtc();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*void DebugTask(void* pvParameters) {
    while (1) {
        if (digitalRead(DEBUG_PIN) == HIGH) {
            debug();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}*/

void ACCMonitorTask(void* pvParameters) {
    for (;;) {
        if (digitalRead(ACC_PIN) == LOW) {
            Serial.println("ACC LOW, preparing for deep sleep...");

            // Ensure clock time is updated before sleep
            lastClockUpdate = esp_timer_get_time();

            // Set up external wakeup on ACC pin HIGH
            esp_sleep_enable_ext0_wakeup((gpio_num_t)ACC_PIN, 1); // Wake when ACC goes HIGH

            // Show sleep message and allow LCD to update
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Sleep...");
            vTaskDelay(pdMS_TO_TICKS(500)); // Let message display and flush serial

            // Use centralized deep sleep routine so sequence is consistent
            enterDeepSleepOnAcc();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void CANUnifiedReceiveTask(void* pvParameters) {
    twai_message_t message;
    while (1) {
        while (twai_receive(&message, 0) == ESP_OK) {
            Serial.print("CAN ID: 0x");
            Serial.print(message.identifier, HEX);
            Serial.print(" DLC: ");
            Serial.print(message.data_length_code);
            Serial.print(" Data: ");
            for (int i = 0; i < message.data_length_code; i++) {
                Serial.print(message.data[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            // Handle valid OBD2 response
            if ((message.identifier >= 0x7E8 && message.identifier <= 0x7EF) &&
                message.data_length_code >= 4 &&
                message.data[1] == 0x41) {
                lastOBD2ResponseTime = millis();

                uint8_t pid = message.data[2];

                // If emulator always sends PID 0x0F, fix it
                if (pid == 0x0F && lastRequestedOBD2PID == PID_CONTROL_MODULE_VOLTAGE &&
                    message.data_length_code >= 5) {
                    int raw = (message.data[3] << 8) | message.data[4];
                    obd2_voltage = raw / 1000.0f;
                    lastCANVoltageTime = millis();
                    Serial.print("OBD2 Voltage (guessed): ");
                    Serial.println(obd2_voltage, 3);
                }
                else if (pid == 0x0F && lastRequestedOBD2PID == PID_COOLANT_TEMP &&
                    message.data_length_code >= 4) {
                    obd2_coolant_temp = message.data[3] - 40;
                    lastCANCoolantTempTime = millis();
                    Serial.print("OBD2 Coolant Temp (guessed): ");
                    Serial.println(obd2_coolant_temp);
                }
                else if (pid == PID_COOLANT_TEMP && message.data_length_code >= 4) {
                    obd2_coolant_temp = message.data[3] - 40;
                    lastCANCoolantTempTime = millis();
                    Serial.print("OBD2 Coolant Temp: ");
                    Serial.println(obd2_coolant_temp);
                }
                else if (pid == PID_CONTROL_MODULE_VOLTAGE && message.data_length_code >= 5) {
                    int raw = (message.data[3] << 8) | message.data[4];
                    obd2_voltage = raw / 1000.0f;
                    lastCANVoltageTime = millis();
                    Serial.print("OBD2 Voltage: ");
                    Serial.println(obd2_voltage, 3);
                }
                else if (pid == PID_ENGINE_RPM && message.data_length_code >= 5) {
                    int raw = (message.data[3] << 8) | message.data[4];
                    obd2_rpm = raw / 4; // Per OBD2 spec
                    Serial.print("OBD2 RPM: ");
                    Serial.println(obd2_rpm);
                }
            }

            else if (message.identifier == CAN_ID_GEAR) {
                can_gear = message.data[3];
                lastCANGearTime = millis();
                Serial.println("Gear Received");
            }

            else if (message.identifier == CAN_ID_VOLTAGE) {
                can_voltage = (message.data[0] << 8) | message.data[1];
                lastCANVoltageTime = millis();
                Serial.print("Voltage (mV): ");
                Serial.println(can_voltage);
            }

        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void OBD2TimeoutTask(void* pvParameters) {
    const unsigned long disconnectTimeout = 2000;
    while (1) {
        if (millis() - lastOBD2ResponseTime > disconnectTimeout) {
            can_connected = false;
            obd2_rpm = 0;
            obd2_coolant_temp = 0;
            can_gear = -1; // Use -1 for "no gear"
            can_voltage = 0;
            Serial.println("OBD2 timeout, resetting values.");
        }
        else {
            can_connected = true;
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}



// --- Existing functions (unchanged) ---

bool receiveOBD2Response() {
    twai_message_t message;
    esp_err_t result = twai_receive(&message, 0);
    if (result == ESP_OK) {
        if (message.identifier == OBD2_RESPONSE_ID &&
            message.data_length_code >= 4 &&
            message.data[1] == 0x41 &&
            message.data[2] == PID_COOLANT_TEMP) {
            obd2_coolant_temp = message.data[3] - 40;
            Serial.print("OBD2 Coolant Temp: ");
            Serial.println(obd2_coolant_temp);
            return true;
        }
    }
    return false;
}

/*void requestOBD2Voltage() {
    lastRequestedOBD2PID = PID_CONTROL_MODULE_VOLTAGE;
    twai_message_t request = { 0 };
    request.identifier = OBD2_REQUEST_ID;
    request.flags = TWAI_MSG_FLAG_NONE;
    request.data_length_code = 8;
    request.data[0] = 0x02;
    request.data[1] = 0x01;
    request.data[2] = PID_CONTROL_MODULE_VOLTAGE;
    for (int i = 3; i < 8; i++) request.data[i] = 0x00;
    twai_transmit(&request, pdMS_TO_TICKS(100));
    Serial.println("OBD2 voltage request sent");
}*/

void displayLcdScreen() {
    int64_t now = esp_timer_get_time();

    if (now - lastTempReadTime >= tempReadInterval) {
        sensors.requestTemperatures();
        lastTempC = sensors.getTempCByIndex(0);
        lastTempReadTime = now;
        Serial.println("Temp collected!");
    }

    float displayTemp = lastTempC;
    if (displayTemp < minDisplayTemp) displayTemp = minDisplayTemp;
    if (displayTemp > maxDisplayTemp) displayTemp = maxDisplayTemp;

    if (firstDisplay || cHour != lastDisplayHour || cMinute != lastDisplayMinute) {
        lcd.setCursor(0, 0);
        char clock[6];
        snprintf(clock, sizeof(clock), "%02d:%02d", cHour, cMinute);
        lcd.print(clock);
        lcd.print("   ");
        lastDisplayHour = cHour;
        lastDisplayMinute = cMinute;
        Serial.println("Clock Changed");
    }

    lcd.setCursor(10, 1);
    lcd.print("T:");

    int tempVal = (int)(lastTempC + 0.5f);  // Use latest temperature reading
    if (tempVal < minDisplayTemp) tempVal = minDisplayTemp;
    if (tempVal > maxDisplayTemp) tempVal = maxDisplayTemp;

    // Optional: Clear old value space (cols 14–16)
    lcd.setCursor(12, 1);
    lcd.print("    ");

    // Right-align temperature value so it ends at col 16
    int digits = String(tempVal).length();
    int numberStartCol = 14 - digits + 1;
    lcd.setCursor(numberStartCol, 1);
    lcd.print(tempVal);

    if (firstDisplay || obd2_coolant_temp != lastDisplayCoolant) {
        lcd.setCursor(0, 1);
        lcd.print("C:");

        // Clamp and prepare the value
        int coolantVal = obd2_coolant_temp;
        if (coolantVal < 0 || coolantVal > 999) coolantVal = 0;

        // Optional: Clear previous number area (cols 4–6)
        lcd.setCursor(2, 1);
        lcd.print("    ");

        // Calculate digit count and align right
        int digits = String(coolantVal).length();
        int numberStartCol = 4 - digits + 1;
        lcd.setCursor(numberStartCol, 1);
        lcd.print(coolantVal);

        lastDisplayCoolant = coolantVal;
        Serial.println("coolant temp collected");
    }

    const char* gearStr = getGearDisplay(can_gear);
    bigNums.setBigNumberCursor(6);
    bigNums.print(gearStr);

    /*lcd.setCursor(10, 0);
    // Clear previous RPM value (up to 6 chars: "R:1234")
    lcd.print("      ");
    lcd.setCursor(10, 0);
    char rpmBuf[10];
    snprintf(rpmBuf, sizeof(rpmBuf), "R:%d", obd2_rpm);
    lcd.print(rpmBuf);*/

    // --- RPM BAR instead of number ---
    lcd.setCursor(10, 0);

    // Clear bar area (6 columns)
    lcd.print("      ");

    // Map RPM (0–8000 for example) to 0–6 blocks
    int barLength = map(obd2_rpm, 0, 5000, 0, 6);

    lcd.setCursor(10, 0);
    for (int i = 0; i < barLength; i++) {
        lcd.write(byte(255));
    }


    firstDisplay = false;

    /*lcd.setCursor(10, 0);
    float voltage = obd2_voltage;
    char voltageBuf[10];
    snprintf(voltageBuf, sizeof(voltageBuf), "v:%.2f", voltage);
    lcd.print(voltageBuf);*/
}

void updateClockFromRtc() {
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - lastClockUpdate;
    if (elapsed > 0 && elapsed < 3600000000LL) {
        int minutesPassed = elapsed / 60000000LL;
        if (minutesPassed > 0) {
            cMinute += minutesPassed;
            if (cMinute > 59) {
                cHour += cMinute / 60;
                cMinute = cMinute % 60;
            }
            if (cHour > 23) {
                cHour = cHour % 24;
            }
            lastClockUpdate = now - (elapsed % 60000000LL);
        }
    }
    else {
        lastClockUpdate = now;
    }
}

/*void debug() {
    if (digitalRead(BUTTON1_PIN) == LOW) {
        Serial.println("Button 1 Debug");
    }
    if (digitalRead(BUTTON2_PIN) == LOW) {
        Serial.println("Button 2 Debug");
    }
    heartBeat++;
    Serial.println("Heart Beat: ");
    Serial.println(heartBeat);
}*/

void enterDeepSleepOnAcc() {
    lastClockUpdate = esp_timer_get_time();
    // Ensure wakeup is configured (caller may already have done this)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)ACC_PIN, 1);
    Serial.println("Entering Deep Sleep, Waiting on ACC HIGH");

    // Clear and give the LCD a short time to process the command
    lcd.clear();
    lcd.home();
    delay(50);

    // Optionally hide characters to reduce visible ghosting immediately
    // (doesn't remove DDRAM but prevents visible flicker)
    lcd.noDisplay();
    delay(20);

    // Drive LCD control/data lines to a known, low state before cutting Vcc
    const int lcdPins[] = { LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7 };
    for (size_t i = 0; i < sizeof(lcdPins) / sizeof(lcdPins[0]); ++i) {
        pinMode(lcdPins[i], OUTPUT);
        digitalWrite(lcdPins[i], LOW);
    }
    delay(10); // let pins settle

    // Turn peripherals off (drive MOSFET gate low).
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, LOW);

    // Hold the pad low during deep sleep so external power stays off.
    gpio_hold_en((gpio_num_t)POWER_PIN);
    gpio_deep_sleep_hold_en();
    delay(10);

    esp_deep_sleep_start();
}

const char* getGearDisplay(int gear) {
    if (gear == 0) return "N";      // Neutral
    if (gear == 255) return "R";    // Reverse (if your protocol uses 255 for reverse)
    if (gear > 0 && gear <= 9) {
        static char buf[2];
        snprintf(buf, sizeof(buf), "%d", gear);
        return buf;
    }
    return "-"; // Unknown/invalid
}

void showWakeupScreen() {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Welcome!");
    lcd.setCursor(0, 1);
    lcd.print("System Starting...");
    delay(2000); // Show for 2 seconds
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("AVDB");
    lcd.setCursor(0, 1);
    char vBuf[16];
    snprintf(vBuf, sizeof(vBuf), "Version: %f", version);
    lcd.print(vBuf);
    delay(2000);
    lcd.clear();
}

// --- RTOS setup ---

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Ensure any previous deep-sleep hold is released first so we can control POWER_PIN now.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)POWER_PIN);

    // Drive power MOSFETs ON immediately so peripherals are powered before we touch the LCD.
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH); // Ensure power pin is HIGH to keep power on

    //pinMode(ACC_PIN, OUTPUT);
    //digitalWrite(ACC_PIN, HIGH);

    // Allow time for the LCD/CAN module power rails to stabilise after the MOSFETs turn on.
    // Increase this if your modules need more time to boot (100-500 ms typical).
    delay(200);

    // Now safe to initialise the LCD and big number helper.
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);

    showWakeupScreen();

    bigNums.begin();

    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(ACC_PIN, INPUT_PULLUP);
    pinMode(DEBUG_PIN, INPUT);

    // Leave POWER_PIN state to be managed in runtime; will be driven LOW before sleeping.
    // (gpio_deep_sleep_hold_dis and gpio_hold_dis already called above)

    lastCANGearTime = millis();
    lastCANVoltageTime = millis();
    lastCANCoolantTempTime = millis();

    if (lastClockUpdate == 0) {
        cHour = hour;
        cMinute = minute;
        lastClockUpdate = esp_timer_get_time();
    }
    else {
        updateClockFromRtc();
    }

    if (digitalRead(ACC_PIN) == LOW) {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)ACC_PIN, 1); // Wake on HIGH
        esp_deep_sleep_start();
    }

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    Serial.print("Wakeup cause: ");
    Serial.println(wakeup_reason);


    if (digitalRead(ACC_PIN) == LOW) {
        enterDeepSleepOnAcc();
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_GPIO, (gpio_num_t)CAN_RX_GPIO, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("TWAI driver installed!");
    }
    if (twai_start() == ESP_OK) {
        Serial.println("TWAI driver started!");
    }

    // Start RTOS tasks
    xTaskCreate(OBD2RequestTask, "OBD2Request", 4096, NULL, 2, NULL);
    xTaskCreate(ButtonTask, "Button", 4096, NULL, 2, NULL);
    xTaskCreate(DisplayTask, "Display", 8192, NULL, 1, NULL);
    xTaskCreate(ClockTask, "Clock", 2048, NULL, 1, NULL);
    //xTaskCreate(DebugTask, "Debug", 2048, NULL, 1, NULL);
    xTaskCreate(CANUnifiedReceiveTask, "CANUnifiedReceiveTask", 8192, NULL, 2, NULL);
    xTaskCreate(ACCMonitorTask, "ACCMonitor", 2048, NULL, 1, NULL);
    xTaskCreate(OBD2TimeoutTask, "OBD2Timeout", 2048, NULL, 1, NULL);

}

void loop() {
    // Not used in RTOS/ARTOS setup
}