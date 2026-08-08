# IoT Based Smart Health Tracking and Air-Purifying System for Hazardous Environment Monitoring

## Project Description

An ESP32-based IoT system designed for real-time hazardous environment and worker health monitoring. The system monitors air quality, carbon monoxide, temperature, humidity, heart rate, and SpO2. It provides automatic alerts and controls an air-purifying fan when hazardous conditions are detected.

## Key Features

- Real-time air quality monitoring
- Carbon monoxide detection
- Temperature and humidity monitoring
- Heart rate monitoring
- SpO2 monitoring
- Automatic buzzer alert
- Automatic fan control for air purification
- 16x2 I2C LCD display
- Blynk-based remote monitoring
- ESP32-based IoT implementation

## System Block Diagram

![System Block Diagram](Block-Diagram-and-Circuit-Diagram.pdf)

## Project Prototype

![Project Prototype](Project-Prototype.jpg)

## Hardware Components

- ESP32
- MQ135 Gas Sensor
- MQ7 Gas Sensor
- DHT11 Temperature and Humidity Sensor
- MAX30100 Pulse Oximeter Sensor
- 16x2 I2C LCD
- Buzzer
- DC Fan
- Power Supply

## Software and Technologies

- Arduino IDE
- Embedded C/C++
- Blynk IoT
- ESP32
- I2C Communication

## Working Principle

The ESP32 collects data from the gas sensors, DHT11, and MAX30100 sensor. The measured values are processed and displayed on the LCD. When hazardous gas levels or high temperature are detected, the system activates the buzzer and automatically turns ON the fan for air purification.

Health parameters such as heart rate and SpO2 are also monitored. The collected data can be sent to the Blynk IoT platform for remote monitoring.

## Project Code

The Arduino source code is available in:

`AirQuality_HealthMonitor.ino`

