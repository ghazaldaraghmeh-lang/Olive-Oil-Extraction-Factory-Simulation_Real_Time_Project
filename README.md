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

# Project File Structure

This section describes the purpose and responsibility of each header file used in the Olive Oil Factory Simulation project.

| File Name | Description |
|---|---|
| `project_contract.h` | Defines the common agreement between team members, including factory stages, batch statuses, the number of pipes, and FIFO names. |
| `messages.h` | Defines the messages exchanged between factory processes and the Supervisor, such as stage started, stage completed, errors, and shutdown notifications. |
| `config.h` | Defines factory configuration settings and constants, such as the number of batches, olive weight, and oil extraction ratio. |
| `quality_control.h` | Declares functions for checking batch quality, calculating the Quality Score, and determining whether the extracted oil is accepted or rejected. |
| `logger.h` | Declares functions for recording factory events, messages, and errors in the log file. |

## Notes

- All header files should contain include guards to prevent multiple inclusions.
- Function declarations should be placed in the appropriate header files, while their implementations should be placed in corresponding `.c` files.
- Shared structures and constants should be defined consistently across the project.
- All team members should follow these definitions to ensure compatibility and smooth integration of their code.

- ## Header Files Organization
In addition to the shared contract header files, the project includes module-specific header files.

**Example:** `display.h` is required for the display module, but it does not need to be part of the shared contract files. 
Each partner is responsible for creating and maintaining the header file related to their own module, provided that all function signatures used by the Core are agreed upon by the team.

### 1. Shared Header Files

These header files are shared across the team, and all partners must follow their definitions and agreements:

```text
project_contract.h
messages.h
config.h
production_input.h          
production_output.h
quality_control.h
logger.h
```

These files form the **Shared Contract** of the project and ensure consistency and compatibility between all modules.

**Responsibilities:**
- **Input Module (production_input.h):** Handles Reception, Cleaning, and Washing stages.
- **Output Module(production_output.h ):** Handles Crushing, Malaxation, and Separation stages.
- **Graphics Module:** Handles the visual representation and display of the factory operations.

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

