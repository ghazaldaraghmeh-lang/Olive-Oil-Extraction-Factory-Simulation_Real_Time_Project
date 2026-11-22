
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "project_contract.h"
#include "config.h"
#include "process_manager.h"
#include "supervisor.h"
#include "display.h"

#define TIMING_FILE "/tmp/olive_factory_timing"
#define OPENMP_TIMING_FILE "/tmp/olive_factory_openmp_timing"

/*
 * Calculate elapsed time in seconds.
 */
static double get_elapsed_time(struct timespec start,
                               struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec)
         + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

/*
 * Read the average Separation calculation time
 * saved by the Separation process.
 */
static double read_openmp_average(void)
{
    FILE *fp = fopen(OPENMP_TIMING_FILE, "r");

    if (fp == NULL)
        return -1.0;

    double average = -1.0;
    int count = 0;

    if (fscanf(fp, "%lf %d", &average, &count) != 2 ||
        count <= 0) {
        average = -1.0;
    }

    fclose(fp);

    return average;
}

static pid_t start_graphics(void)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("[Main] fork OpenGL display");
        return -1;
    }

    if (pid == 0) {
        struct sigaction sa;

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = SIG_DFL;

        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        char *gui_argv[] = { (char *)"olive_gui", NULL };

        display_init(1, gui_argv);
        display_run();
        display_close();

        _exit(EXIT_SUCCESS);
    }

    return pid;
}

/*
 * Used only if factory process creation fails.
 * In this case, the GUI must be terminated.
 */
static void stop_graphics(pid_t pid)
{
    if (pid <= 0)
        return;

    if (kill(pid, SIGTERM) == -1 && errno != ESRCH)
        perror("[Main] stop OpenGL display");

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno == EINTR)
            continue;

        if (errno == ECHILD)
            break;

        break;
    }

    unlink(DISPLAY_FIFO);
}

/*
 * Normal operation:
 * Wait until the user closes the GUI window or presses ESC.
 */
static void wait_for_graphics(pid_t pid)
{
    if (pid <= 0)
        return;

    printf("\n[OpenGL] Factory results are displayed.\n");
    printf("[OpenGL] Close the graphics window or press ESC to exit.\n");

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno == EINTR)
            continue;

        if (errno == ECHILD)
            break;

        perror("[Main] waitpid graphics");
        break;
    }

    printf("[OpenGL] Graphics window closed.\n");
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

    ProcessInfo processes[NUM_STAGES];
    int pipes[NUM_PIPES][2];
    int qc_pipe[2];

    FactoryConfig config;
    pid_t graphics_pid = -1;

    const char *config_file = DEFAULT_CONFIG_FILE;

    struct timespec factory_start;
    struct timespec factory_end;

    double total_factory_time = 0.0;

    /*
     * Remove timing information from previous runs.
     */
    unlink(TIMING_FILE);
    unlink(OPENMP_TIMING_FILE);

    if (argc > 1)
        config_file = argv[1];

    /*
     * Production children use the same configuration path.
     */
    if (setenv("OLIVE_CONFIG_FILE", config_file, 1) == -1) {
        perror("[Main] setenv OLIVE_CONFIG_FILE");
        return EXIT_FAILURE;
    }

    printf("\n");
    printf("============================================================\n");
    printf("                  OLIVE OIL FACTORY\n");
    printf("                   SIMULATION SYSTEM\n");
    printf("============================================================\n\n");

    printf("[SYSTEM] Initializing factory...\n");
    printf("[CONFIG] Configuration file: %s\n", config_file);

    for (int i = 0; i < NUM_STAGES; i++) {
        processes[i].pid = -1;
        processes[i].stage = i;
    }

    /*
     * Load factory configuration.
     */
    if (load_config(config_file, &config) != 0) {
        fprintf(stderr,
                "[Main] ERROR: Failed to load configuration file '%s'\n",
                config_file);

        return EXIT_FAILURE;
    }

    printf("\n");
    printf("============================================================\n");
    printf("                  FACTORY CONFIGURATION\n");
    printf("============================================================\n");

    print_config(&config);

    printf("============================================================\n");

    /*
     * Initialize Supervisor.
     */
    if (supervisor_init() != 0) {
        fprintf(stderr,
                "[Main] ERROR: Supervisor initialization failed\n");

        return EXIT_FAILURE;
    }

    /*
     * Start OpenGL in a separate process.
     */
    graphics_pid = start_graphics();

    if (graphics_pid > 0) {
        printf("[GRAPHICS] OpenGL display started (PID %ld).\n",
               (long)graphics_pid);
    } else {
        printf("[OpenGL] WARNING: Graphics process could not be started.\n");
    }

    /*
     * Start measuring factory execution time
     * immediately before creating production processes.
     */
    if (clock_gettime(CLOCK_MONOTONIC, &factory_start) == -1) {
        perror("[Main] clock_gettime start");
        supervisor_shutdown(processes);
        stop_graphics(graphics_pid);
        return EXIT_FAILURE;
    }

    /*
     * Create factory production processes.
     */
    if (create_factory_processes(
            processes,
            pipes,
            qc_pipe,
            &config) != 0) {

        fprintf(stderr,
                "[Main] ERROR: Failed to create factory processes\n");

        supervisor_shutdown(processes);
        stop_graphics(graphics_pid);

        return EXIT_FAILURE;
    }

    printf("\n");
    printf("============================================================\n");
    printf("                    PRODUCTION STARTED\n");
    printf("============================================================\n\n");

    printf("[IPC] Production pipes initialized: 5 + 1 QC pipe\n");
    printf("[IPC] Status communication enabled\n");
    printf("[OPENMP] Separation parallel mode: %s\n",
           config.separation_use_openmp ? "ENABLED" : "DISABLED");

    printf("------------------------------------------------------------\n");

    /*
     * Monitor production processes.
     */
    supervisor_run(processes);

    /*
     * Finish production safely.
     * Supervisor sends the final GUI notification from supervisor_run().
     */
    supervisor_shutdown(processes);

    /*
     * Stop the factory timer after all production
     * processes have completed.
     */
    if (clock_gettime(CLOCK_MONOTONIC, &factory_end) == -1) {
        perror("[Main] clock_gettime end");
    } else {
        total_factory_time =
            get_elapsed_time(factory_start, factory_end);
    }

    printf("\n");
    printf("============================================================\n");
    printf("                  PRODUCTION COMPLETED\n");
    printf("============================================================\n");

    printf("[SYSTEM] Final production results are available.\n");

    /*
     * Display total factory execution time.
     */
    printf("\n");
    printf("============================================================\n");
    printf("                  FACTORY EXECUTION TIME\n");
    printf("============================================================\n");

    printf("[TIME] Total factory time: %.6f seconds\n",
           total_factory_time);

    printf("============================================================\n");

    /*
     * Display OpenMP performance results.
     * The average is calculated from successful
     * Separation yield calculations only.
     */
    printf("\n");
    printf("============================================================\n");
    printf("                  OPENMP PERFORMANCE\n");
    printf("============================================================\n");

    printf("[OpenMP] Mode: %s\n",
           config.separation_use_openmp ? "ENABLED" : "DISABLED");

    double openmp_average = read_openmp_average();

    if (openmp_average >= 0.0) {
        printf("[OpenMP] Average calculation time: %.6f seconds\n",
               openmp_average);
    } else {
        printf("[OpenMP] Average calculation time: Not available\n");
    }

    printf("============================================================\n");

    printf("------------------------------------------------------------\n");

    /*
     * IMPORTANT:
     * Do not kill the GUI after normal production.
     * Wait until the user closes its window.
     */
    wait_for_graphics(graphics_pid);

    unlink(TIMING_FILE);
    unlink(OPENMP_TIMING_FILE);

    return EXIT_SUCCESS;
}
