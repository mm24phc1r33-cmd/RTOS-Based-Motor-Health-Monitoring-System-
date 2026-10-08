# RTOS-Based Motor Health Monitoring System

A real-time embedded motor health monitoring system developed using **STM32F401RE, FreeRTOS, ESP32, MQTT, and Node-RED**.

The system monitors motor parameters such as temperature, current, and vibration, performs threshold-based fault detection, determines the motor operating state, and transmits the monitoring data to an ESP32 gateway for MQTT-based remote monitoring.

---

## Project Overview

The project demonstrates an **RTOS-based embedded monitoring architecture** where the STM32F401RE acts as the main processing unit and ESP32 acts as the wireless communication gateway.

The STM32 runs multiple FreeRTOS tasks for:

- Sensor data handling
- Motor health monitoring
- Fault detection
- Motor state management
- UART communication

The ESP32 receives the data through UART, connects to Wi-Fi, converts the data into JSON format, and publishes it using MQTT.

Node-RED is used for receiving and visualizing the MQTT data.

---

## System Architecture

```text
                 ┌──────────────────────────┐
                 │       STM32F401RE        │
                 │                          │
                 │       FreeRTOS           │
                 │                          │
                 │  ┌────────────────────┐  │
                 │  │ Sensor Task        │  │
                 │  └────────────────────┘  │
                 │  ┌────────────────────┐  │
                 │  │ Fault Task         │  │
                 │  └────────────────────┘  │
                 │  ┌────────────────────┐  │
                 │  │ Communication Task │  │
                 │  └────────────────────┘  │
                 │                          │
                 │ Fault Detection           │
                 │ State Machine              │
                 └────────────┬─────────────┘
                              │
                         UART 115200
                              │
                              ▼
                 ┌──────────────────────────┐
                 │          ESP32           │
                 │                          │
                 │      Wi-Fi Gateway       │
                 │          │               │
                 │          ▼               │
                 │         MQTT             │
                 └────────────┬─────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │    MQTT Broker   │
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │     Node-RED     │
                    │                  │
                    │ Monitoring /     │
                    │ Visualization     │
                    └──────────────────┘

Hardware Used
STM32 NUCLEO-F401RE
ESP32-WROOM-32 / ESP32 DevKit
USB-to-TTL converter
ST-LINK
Jumper wires
Common GND connection

Note: Physical motor, vibration sensor, and current sensor hardware were not available during development. Sensor values were therefore simulated in firmware for validating the RTOS, fault detection, state machine, UART, MQTT, and monitoring pipeline.

Software & Tools
STM32CubeIDE
STM32CubeMX
Embedded C
FreeRTOS
CMSIS-RTOS V2
Arduino IDE
ESP32
MQTT
Node-RED
FlowFuse Dashboard
X-CTU
ST-LINK Debugger
STM32 Pin Configuration
USART1 — STM32 ↔ ESP32
STM32 Pin	Function	ESP32 Pin
PA9	USART1 TX	GPIO16 RX2
PA10	USART1 RX	GPIO17 TX2
GND	Ground	GND

Baud Rate:

115200
8 Data Bits
No Parity
1 Stop Bit
USART2 — STM32 ↔ USB-TTL
STM32 Pin	Function
PA2	USART2 TX
PA3	USART2 RX
GND	Ground

USART2 is used for debugging and monitoring through X-CTU.

LED
STM32 Pin	Function
PA5	Onboard LED
FreeRTOS Task Architecture
1. Sensor Task

The Sensor Task updates the monitoring parameters periodically.

Current simulated values:

Temperature = 28 °C
Humidity    = 62 %
Current     = 2.50 A
Vibration   = 1.80 g

The task runs every:

1000 ms
2. Fault Detection Task

The Fault Task checks the sensor parameters against predefined thresholds.

Thresholds:

Parameter	Warning	Fault
Temperature	≥ 60 °C	≥ 80 °C
Current	≥ 5 A	≥ 7 A
Vibration	≥ 3 g	≥ 5 g

The motor state is classified as:

RUNNING
WARNING
FAULT

The task executes every:

500 ms
3. Communication Task

The Communication Task creates a structured sensor packet and sends it to the ESP32 through USART1.

Example:

SENSOR_DATA,TEMP=28,HUM=62,CURRENT=2.50,VIBRATION=1.80,STATE=RUNNING,ERROR=0

The communication task executes every:

2000 ms
Motor State Machine

The motor state is determined according to the sensor thresholds.

                 ┌───────────┐
                 │  RUNNING  │
                 └─────┬─────┘
                       │
                 Warning Limit
                       │
                       ▼
                 ┌───────────┐
                 │  WARNING  │
                 └─────┬─────┘
                       │
                  Fault Limit
                       │
                       ▼
                 ┌───────────┐
                 │   FAULT   │
                 └───────────┘

The system also maintains an error counter whenever a motor fault condition is detected.

UART Communication

STM32 sends monitoring data to ESP32 using USART1.

Example:

STM32 -> ESP32 :
SENSOR_DATA,TEMP=28,HUM=62,CURRENT=2.50,VIBRATION=1.80,STATE=RUNNING,ERROR=0

ESP32 validates the packet and sends an acknowledgement:

ESP32 -> STM32 :
ACK,SENSOR_DATA
ESP32 JSON Format

After receiving the STM32 packet, the ESP32 converts the information into JSON.

Example:

{
  "temperature": 28,
  "humidity": 62,
  "current": 2.50,
  "vibration": 1.80,
  "state": "RUNNING",
  "error": 0
}
MQTT Communication

The ESP32 publishes the monitoring data using MQTT.

Main Topic
industrial/motor/data
State Topic
industrial/motor/state
Fault Topic
industrial/motor/fault
Gateway Status Topic
industrial/motor/status

The MQTT data was verified using Node-RED.

Node-RED Monitoring

Node-RED receives the MQTT messages and displays the motor monitoring data.

Example received payload:

temperature : 28
humidity    : 62
current     : 2.5
vibration   : 1.8
state       : RUNNING
error       : 0
Fault Detection Testing

The fault detection logic was validated using simulated sensor values.

Normal Condition
Temperature = 28 °C
Current     = 2.50 A
Vibration   = 1.80 g

State = RUNNING
Error = 0
Warning Condition

Temperature was changed to:

65 °C

Expected result:

MOTOR STATE CHANGED: RUNNING -> WARNING
!!! MOTOR WARNING !!!
Fault Condition

Temperature was changed to:

85 °C

Expected result:

MOTOR STATE CHANGED: RUNNING -> FAULT
!!! MOTOR FAULT DETECTED !!!
Error Count = 1

STM32 then transmits:

SENSOR_DATA,TEMP=85,HUM=62,CURRENT=2.50,VIBRATION=1.80,STATE=FAULT,ERROR=1

The ESP32 receives the fault status and publishes it through MQTT.

Key Embedded Concepts Demonstrated
Embedded C
STM32F401RE
FreeRTOS
CMSIS-RTOS V2
RTOS task creation
Task scheduling
Task priorities
Periodic tasks
UART communication
ESP32 interfacing
Fault detection
State machine implementation
Threshold-based monitoring
Error counting
MQTT communication
JSON data formatting
IoT gateway architecture
Node-RED monitoring
Debugging using USB-TTL and X-CTU
Project Workflow
Simulated Sensor Data
        ↓
STM32F401RE
        ↓
FreeRTOS Tasks
        ↓
Fault Detection
        ↓
Motor State Machine
        ↓
UART
        ↓
ESP32
        ↓
Wi-Fi
        ↓
MQTT
        ↓
Node-RED
        ↓
Real-Time Monitoring
Future Improvements

The system can be extended with:

Real temperature sensor
Real current sensor
Real vibration/accelerometer sensor
SD card data logging
RTC-based timestamping
RS485 communication
Modbus RTU
CAN communication
Remote motor control
Advanced vibration analysis
Predictive maintenance algorithms
Machine learning-based anomaly detection
Project Outcome

This project demonstrates the design of a real-time embedded monitoring and IoT gateway using STM32 and FreeRTOS. It combines RTOS-based task management, fault detection, state-machine design, UART communication, ESP32 connectivity, MQTT, and Node-RED monitoring into a complete embedded system workflow.
