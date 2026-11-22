# Olive Oil Extraction Factory Simulation

## Project Information

**Course:** ENCS4330 – Real-Time Applications & Embedded Systems  
**Semester:** First Semester 2026/2027  
**Project:** Project #1 – Signals, Pipes & FIFOs under Linux

## Project Description

This project implements a simulation of an olive oil extraction factory using Linux system programming concepts.

The system simulates the main stages of the olive oil production process, starting from receiving olives and ending with oil separation and quality control.

The factory is designed as a collection of cooperating processes. Each process is responsible for a specific production stage, while a central Supervisor process coordinates the entire system, monitors its operation, handles signals, and manages safe shutdown.

## Factory Simulation

The simulation includes the following stages:

1. **Reception:** Receives olive batches and monitors the availability of olives using a container sensor.
2. **Cleaning:** Removes leaves and other unwanted materials using an air-blower simulation.
3. **Washing:** Simulates washing olives before processing.
4. **Crushing:** Crushes olives to produce olive paste.
5. **Malaxation:** Mixes the olive paste to prepare it for separation.
6. **Pressing and Separation:** Separates the olive paste into olive oil, vegetable water, and pomace.
7. **Quality Control:** Monitors production parameters such as temperature, pressure, and processing time.
8. **Supervisor:** Coordinates processes, handles communication and signals, monitors system status, records events, and manages safe shutdown.

## Technologies and Concepts

The project uses the following technologies and concepts:

- **C Programming Language:** Implementing the simulation and system modules.
- **Linux:** Operating environment.
- **Multiprocessing (`fork()`):** Running different factory stages as independent processes.
- **Signals:** Handling alarms, operational events, and process termination.
- **Pipes:** Enabling communication and data exchange between processes.
- **FIFOs:** Named pipes, where appropriate, for inter-process communication.
- **OpenMP:** Parallelizing selected computational tasks and comparing execution performance.
- **OpenGL:** Providing a simple graphical representation of the factory simulation.
- **GDB:** Debugging and testing the program.
- **Makefile:** Automating the compilation process.

## Team Members and Responsibilities

The project is divided among four team members to support parallel development and easier integration.

| Team Member | Responsibility | Main Tasks |
|---|---|---|
| Member 1 – [Ghazal] | System Core & Supervisor | Main program, process creation and management, IPC setup, signals, process monitoring, and safe shutdown. |
| Member 2 – [Waad ] | Production Input Stages | Reception, cleaning, and washing modules. |
| Member 3 – [Sadeel] | Production Output Stages | Crushing, malaxation, separation, and OpenMP implementation and performance comparison. |
| Member 4 – [Maisam] | Quality Control, Configuration & GUI | Quality control, configuration file handling, graphical interface using OpenGL, and event reporting support. |

## Project File Structure

This section describes the shared header files, their responsibilities, and how they ensure consistency and compatibility between project modules.

### 1. Shared Header Files

The following header files form the **Shared Contract** of the Olive Oil Factory Simulation project. All team members must follow their definitions and agreements to ensure smooth integration and avoid compatibility issues.

| Header File | Responsibility |
|---|---|
| `project_contract.h` | Defines common agreements, including factory stages, batch statuses, number of pipes, and FIFO names. |
| `messages.h` | Defines messages exchanged between factory processes and the Supervisor, including stage events, errors, and shutdown notifications. |
| `config.h` | Defines factory configuration settings and constants, such as batch size and oil extraction ratio. |
| `production_input.h` | Declares functions for the input stages: Reception, Cleaning, and Washing. |
| `production_output.h` | Declares functions for the output stages: Crushing, Malaxation, and Separation. |
| `parallel_processing.h` | Declares functions related to parallel processing and OpenMP-based calculations. |
| `process_manager.h` | Declares functions responsible for managing and coordinating factory processes. |
| `supervisor.h` | Declares functions for monitoring production stages and supervising factory operations. |
| `display.h` | Declares functions responsible for the graphical representation and display of factory operations. |
| `quality_control.h` | Declares functions for evaluating batch quality, calculating quality scores, and determining acceptance or rejection. |
| `logger.h` | Declares functions for recording factory events, messages, and errors. |
| `sensors.h` | Declares functions for monitoring and updating factory sensor readings, including temperature and pressure. |

### 2. Module Responsibilities

The project is organized into the following main modules:

- **Input Module:** Handles the initial production stages: Reception → Cleaning → Washing.
- **Output Module:** Handles the final production stages: Crushing → Malaxation → Separation.
- **Core Module:** Manages process creation, coordination, and overall factory supervision.
- **Monitoring Module:** Handles configuration, sensor monitoring, event logging, and quality control.
- **Graphics Module:** Provides the visual representation of production stages, factory status, sensor readings, and production statistics.
- **Parallel Processing Module:** Uses OpenMP to accelerate separation calculations.

### 3. Development Guidelines

- All header files must include include guards to prevent multiple inclusions.
- Function declarations must be placed in the appropriate header files, while implementations belong in their corresponding `.c` files.
- Shared structures, constants, and function interfaces must remain consistent across all modules.
- Any changes to shared interfaces must be agreed upon by the team before integration.
- All team members must follow the shared definitions to ensure compatibility and reliable integration.


## Project Goals

- Simulate the main stages of an olive oil extraction factory.
- Apply Linux multiprocessing concepts.
- Implement communication between processes.
- Handle signals and system events.
- Monitor production conditions and detect abnormal situations.
- Apply parallel processing using OpenMP.
- Visualize the factory operation using OpenGL.
- Test, debug, and evaluate the system.

## Repository Structure

The repository will contain the source code, header files, configuration file, Makefile, and project documentation.

The directory structure will be organized into modules according to the responsibilities of each team member.

---

**Project Team:** [Ghazal Salah,
Maisam Alawi,
Waab Abu Rukab,
Sadeel Hamouda
]

