# ESP32 Mini S2 LCD Dashboard

A real-time automotive data dashboard for ESP32 Mini S2 microcontroller that displays vehicle information on a 16x2 LCD screen for Motorcycles.

Hello to all, This is a personal project of mine to learn to program Microcontrollers and make something functional and useful for practical use. 
Inspiration initially started with the display of my fathers motorcycle and car, both having radial dials for speed but also the motorcycle lacking 
useful display information such as gear or coolant. So I started to learn how to create a digital display for him.

I have created two slightly different codes, one for a Motorcycle that displays Gear, RPM, Coolant Temp, Ambient temp and Voltage. Then the Car
displays the MPH, Gear, Coolant Temp, Voltage and RPM

This is the Motorcycle Dashboard

I would be happy to accept and suggestions from those that might be more experienced than me or might have interesting ideas as to making my code more efficient and potentially effective. People are also welcome to test out the program and respond to me about their results and issues.

Thank you

## Features

- **Gear Position Display** - Shows current transmission gear
- **Coolant Temperature Monitoring** - Tracks engine coolant temperature
- **Digital Clock** - Built-in RTC clock display
- **CANbus Integration** - Reads vehicle data through CANbus module
- **FreeRTOS Multitasking** - Efficient task scheduling and management
- **Button Controls** - Physical buttons for Clock Adjustment with short press and long press capability.

## Hardware Components

- **Microcontroller**: ESP32 Mini S2
- **Display**: 16x2 LCD Screen
- **Communication**: CANbus Module
- **Power Management**: Buck Converter
- **Input**: Buttons for user control

## Software

- **Language**: Arduino (C++)
- **OS**: FreeRTOS
- **Build Environment**: Arduino IDE(for upload and libraries), Visual Studio for code writing

## Getting Started

1. Connect the CANbus module to the ESP32 Mini S2
2. Wire the 16x2 LCD display to the microcontroller
3. Connect the buck converter for power regulation
4. Install required Arduino libraries
5. Upload the sketch to your ESP32 Mini S2

## Project Structure

- Main sketch file contains core functionality
- FreeRTOS tasks handle concurrent operations
- CANbus module reads vehicle data
- LCD driver manages display output

## License

This project is open source and available under the MIT License.

## Author

Surtrofthe9realms
