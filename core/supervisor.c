
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "supervisor.h"
#include "project_contract.h"
#include "messages.h"
#include "process_manager.h"
#include "logger.h"
#include "display.h"

static volatile sig_atomic_t shutdown_requested = 0;

static int logger_initialized = 0;
static int status_fifo_fd = -1;


static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        shutdown_requested = 1;
    }
}


/* Initialize Supervisor, STATUS_FIFO, signal handlers and logger */
int supervisor_init(void)
{
    struct sigaction sa;

    if (unlink(STATUS_FIFO) == -1 && errno != ENOENT) {
        perror("[Supervisor] unlink old STATUS_FIFO");
        return -1;
    }

    if (mkfifo(STATUS_FIFO, 0666) == -1) {
        perror("[Supervisor] mkfifo STATUS_FIFO");
        return -1;
    }

    status_fifo_fd = open(
        STATUS_FIFO,
        O_RDWR | O_NONBLOCK
    );

    if (status_fifo_fd == -1) {
        perror("[Supervisor] open STATUS_FIFO");
        unlink(STATUS_FIFO);
        return -1;
    }

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {

        perror("[Supervisor] sigaction");

        close(status_fifo_fd);
        status_fifo_fd = -1;

        unlink(STATUS_FIFO);

        return -1;
    }

    if (logger_init() == 0) {
        logger_initialized = 1;
        log_info("Supervisor initialized successfully");
    } else {
        fprintf(stderr,
                "[Supervisor] WARNING: Logger initialization failed.\n");
    }

    shutdown_requested = 0;

printf("\n[SUPERVISOR] Initialized successfully.\n");

    return 0;
}


/*
 * Forward a production event to the OpenGL display.
 */
static void forward_to_graphics(const ProcessMessage *msg)
{
    if (msg == NULL) {
        return;
    }

    int fd = open(DISPLAY_FIFO, O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        /*
         * The GUI may not be available or may already be closed.
         * Factory production must continue in this case.
         */
        return;
    }

    ssize_t n;

    do {
        n = write(fd, msg, sizeof(*msg));
    } while (n < 0 && errno == EINTR);

    if (n != (ssize_t)sizeof(*msg)) {
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("[Supervisor] write DISPLAY_FIFO");
        } else if (n >= 0) {
            fprintf(stderr,
                    "[Supervisor] WARNING: Incomplete display message.\n");
        }
    }

    close(fd);
}


/*
 * Notify GUI that production has finished.
 *
 * status = 0 -> normal completion
 * status = 1 -> emergency shutdown
 */
static void notify_graphics_finished(void)
{
    ProcessMessage msg;

    memset(&msg, 0, sizeof(msg));

    msg.process_id = getpid();
    msg.stage_id = STAGE_SUPERVISOR;
    msg.message_type = MSG_FACTORY_SHUTDOWN;
    msg.batch_id = 0;

    if (shutdown_requested) {
        msg.status = 1;

        snprintf(
            msg.message,
            sizeof(msg.message),
            "Emergency shutdown - Press ESC to close"
        );
    } else {
        msg.status = 0;

        snprintf(
            msg.message,
            sizeof(msg.message),
            "Factory completed successfully - Press ESC to close"
        );
    }

    forward_to_graphics(&msg);


}


static const char *get_stage_name(int stage)
{
    switch (stage) {
        case STAGE_RECEPTION:
            return "RECEPTION";

        case STAGE_CLEANING:
            return "CLEANING";

        case STAGE_WASHING:
            return "WASHING";

        case STAGE_CRUSHING:
            return "CRUSHING";

        case STAGE_MALAXATION:
            return "MALAXATION";

        case STAGE_SEPARATION:
            return "SEPARATION";

        case STAGE_SUPERVISOR:
            return "SUPERVISOR";

        default:
            return "SYSTEM";
    }
}



/*
 * Print and log a received production event.
 */
static void print_message(const ProcessMessage *msg)
{
    if (msg == NULL) {
        return;
    }

    const char *stage_name = get_stage_name(msg->stage_id);

    if (msg->batch_id > 0) {
        printf("[%-11s] Batch %-3d | %s",
               stage_name,
               msg->batch_id,
               msg->message);
    } else {
        printf("[%-11s] %s",
               stage_name,
               msg->message);
    }

    /*
     * Display sensor readings only when actual values exist.
     * This avoids printing meaningless T=0 and P=0 values.
     */
       printf("\n");
    fflush(stdout);

    if (logger_initialized) {
        log_process_message(msg);
    }

    forward_to_graphics(msg);
}


/*
 * Read and process all messages still waiting in STATUS_FIFO.
 *
 * This function is called after all production processes have
 * finished, so their final messages should already be in the FIFO.
 */
static void drain_status_fifo(void)
{
    if (status_fifo_fd < 0) {
        return;
    }

    printf("[Supervisor] Processing remaining FIFO messages...\n");

    for (;;) {
        ProcessMessage msg;

        ssize_t n = read(
            status_fifo_fd,
            &msg,
            sizeof(msg)
        );

        if (n == (ssize_t)sizeof(msg)) {
            print_message(&msg);
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        if (n == 0) {
            break;
        }

        if (n > 0) {
            fprintf(stderr,
                    "[Supervisor] WARNING: Incomplete message "
                    "while draining STATUS_FIFO.\n");
        } else if (n < 0) {
            perror("[Supervisor] drain STATUS_FIFO");
        }

        break;
    }
}


/*
 * Check child processes without blocking.
 *
 * Returns 1 when all production processes have finished.
 * Returns 0 while at least one process is still active.
 */
static int check_processes_finished(ProcessInfo processes[])
{
    int active = 0;

    for (int i = 0; i < STAGE_SUPERVISOR; i++) {

        if (processes[i].pid <= 0) {
            continue;
        }

        int status;

        pid_t result = waitpid(
            processes[i].pid,
            &status,
            WNOHANG
        );

        if (result == 0) {

            active++;

        } else if (result == processes[i].pid) {

            printf("[PROCESS] %-11s (PID %d) finished.\n",
       get_stage_name(processes[i].stage),
       processes[i].pid);

            processes[i].pid = -1;

        } else if (result == -1) {

            if (errno == ECHILD) {

                processes[i].pid = -1;

            } else if (errno == EINTR) {

                active++;

            } else {

                perror("[Supervisor] waitpid");
                active++;
            }
        }
    }

    return active == 0;
}


/*
 * Supervisor monitoring loop.
 */
void supervisor_run(ProcessInfo processes[])
{
    if (processes == NULL) {
        return;
    }

    if (status_fifo_fd == -1) {
        fprintf(stderr,
                "[Supervisor] STATUS_FIFO is not open.\n");
        return;
    }

printf("[SUPERVISOR] Monitoring production events...\n");

    while (!shutdown_requested) {

        ProcessMessage msg;

        ssize_t n = read(
            status_fifo_fd,
            &msg,
            sizeof(msg)
        );

        if (n == (ssize_t)sizeof(msg)) {

            print_message(&msg);

        } else if (n == -1) {

            if (errno != EAGAIN &&
                errno != EWOULDBLOCK &&
                errno != EINTR) {

                perror("[Supervisor] FIFO read");
                shutdown_requested = 1;
                break;
            }

        } else if (n > 0) {

            fprintf(
                stderr,
                "[Supervisor] WARNING: Incomplete message received.\n"
            );
        }

        /*
         * Automatically detect factory completion.
         */
        if (check_processes_finished(processes)) {

            /*
             * All children have exited. Process any messages
             * they wrote before exiting, before notifying the GUI.
             */
            drain_status_fifo();

            printf("\n");
printf("[SUPERVISOR] All production processes completed successfully.\n");

            if (logger_initialized) {
                log_info(
                    "All production processes completed successfully"
                );
            }

            break;
        }

        /*
         * Avoid busy waiting.
         */
        struct timespec ts = {0, 100000000L};

        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
            if (shutdown_requested) {
                break;
            }
        }
    }

    /*
     * Send final notification after draining pending messages
     * in the normal completion case.
     */
    notify_graphics_finished();

    if (status_fifo_fd >= 0) {
        close(status_fifo_fd);
        status_fifo_fd = -1;
    }

    if (shutdown_requested) {
        printf("\n[Supervisor] Shutdown signal received.\n");

        if (logger_initialized) {
            log_info("Emergency shutdown requested");
        }
    }
}


/*
 * Safely shut down factory processes and logger.
 */
void supervisor_shutdown(ProcessInfo processes[])
{
    if (processes == NULL) {
        return;
    }

    /*
     * Terminate active production children only during
     * emergency shutdown.
     */
    if (shutdown_requested) {

        printf("[Supervisor] Terminating active factory processes...\n");

        for (int i = 0; i < STAGE_SUPERVISOR; i++) {

            if (processes[i].pid > 0) {

                if (kill(processes[i].pid, SIGTERM) == -1) {

                    if (errno != ESRCH) {
                        perror("[Supervisor] kill");
                    }

                } else {

                    printf(
                        "[Supervisor] SIGTERM sent to stage %d (PID %d).\n",
                        processes[i].stage,
                        processes[i].pid
                    );
                }
            }
        }
    }

    /*
     * Reap all remaining production processes.
     */
    wait_for_processes(processes);

printf("[SUPERVISOR] Factory processes shut down safely.\n");
    /*
     * Close logger.
     */
    if (logger_initialized) {

        if (shutdown_requested) {
            log_info(
                "Factory safely shut down after emergency request"
            );
        } else {
            log_info(
                "Factory shutdown completed normally"
            );
        }

        logger_close();
        logger_initialized = 0;
    }

    /*
     * Close and remove STATUS_FIFO.
     *
     * Do NOT remove DISPLAY_FIFO here.
     * The GUI process removes it when the user closes the window.
     */
    if (status_fifo_fd >= 0) {
        close(status_fifo_fd);
        status_fifo_fd = -1;
    }

    if (unlink(STATUS_FIFO) == 0) {
        printf("[IPC] STATUS_FIFO cleaned up.\n");
    } else if (errno != ENOENT) {
        perror("[Supervisor] unlink STATUS_FIFO");
    }
}
