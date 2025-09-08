# Health Monitor System  

An IoT-based system for real-time health monitoring using the Tiva C Series microcontroller and FreeRTOS. The project integrates body temperature and heart rate sensors, transmits data via ESP8266 to Firebase, and provides both local and remote access through LCD display and a mobile interface.  

---

## Project Info
**Date:** Spring 2025  

---

## Features

### Sensor Integration
- Body temperature and heart rate acquisition using Tiva C Series.  
- Real-time data processing and accuracy validation.  

### RTOS-Based Task Management
- FreeRTOS tasks for sensor data acquisition, processing, communication, and display.  

### Cloud Communication
- UART communication between Tiva C and ESP8266 NodeMCU.  
- Data transmission to Firebase for real-time logging and remote access.  

### User Interface
- Local LCD display for immediate data output.  
- Remote monitoring via a simple mobile application.  

### Alert System
- Automatic notifications when health parameters deviate from normal ranges.  

---

## Tech Stack
- **Language:** C (Embedded development)  
- **Frameworks:** FreeRTOS  
- **Hardware:** Tiva C Series, ESP8266 NodeMCU, LCD  
- **Cloud:** Firebase  
