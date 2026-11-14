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
| Member 1 – [Name] | System Core & Supervisor | Main program, process creation and management, IPC setup, signals, process monitoring, and safe shutdown. |
| Member 2 – [Name] | Production Input Stages | Reception, cleaning, and washing modules. |
| Member 3 – [Name] | Production Output Stages | Crushing, malaxation, separation, and OpenMP implementation and performance comparison. |
| Member 4 – [Name] | Quality Control, Configuration & GUI | Quality control, configuration file handling, graphical interface using OpenGL, and event reporting support. |

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

**Project Team:** [Ghazal Salah
Maisam Alawi
Waab Abu Rukab
Sadeel Hamouda
]

