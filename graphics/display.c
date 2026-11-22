
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <GL/freeglut.h>

#include "display.h"

/* Stage names in GUI display order */
static const char *stage_names[NUM_PRODUCTION_STAGES] = {
    "Reception",
    "Cleaning",
    "Washing",
    "Separation",
    "Malaxation",
    "Crushing"
};

/* Stage statuses */
static StageStatus stage_status[NUM_PRODUCTION_STAGES];

/* Factory data */
static FactoryStatistics factory_stats = {0};
static FactorySensors factory_sensors = {0};

static int display_initialized = 0;
static int factory_finished = 0;
static int display_fifo_fd = -1;

/* Factory alerts */
static AlertType current_alert_type = ALERT_NONE;
static char current_alert_message[MAX_MESSAGE] = "";

/*
 * Track batches that already produced a final result.
 * Each batch must be counted only once.
 */
typedef struct BatchRecord {
    int batch_id;
    float oil_quantity;
    float water_quantity;
    float pomace_quantity;
    struct BatchRecord *next;
} BatchRecord;

static BatchRecord *counted_batches = NULL;
static int max_batch_id = 0;

/* Buffer for complete ProcessMessage records from FIFO */
static unsigned char fifo_buffer[sizeof(ProcessMessage)];
static size_t fifo_bytes_used = 0;


/* Check whether a batch was already counted */
static int batch_already_counted(int batch_id)
{
    if (batch_id <= 0) {
        fprintf(stderr,
                "[Display] WARNING: Invalid batch ID %d. "
                "Statistics were not updated.\n",
                batch_id);
        return 1;
    }

    BatchRecord *current = counted_batches;

    while (current != NULL) {
        if (current->batch_id == batch_id) {
            printf("[Display] Duplicate final result ignored "
                   "for Batch %d.\n",
                   batch_id);
            return 1;
        }

        current = current->next;
    }

    BatchRecord *record = malloc(sizeof(BatchRecord));

    if (record == NULL) {
        fprintf(stderr,
                "[Display] ERROR: Cannot track Batch %d. "
                "Statistics were not updated.\n",
                batch_id);
        return 1;
    }

    record->batch_id = batch_id;
    record->oil_quantity = 0.0f;
    record->water_quantity = 0.0f;
    record->pomace_quantity = 0.0f;
    record->next = counted_batches;
    counted_batches = record;

    if (batch_id > max_batch_id) {
        max_batch_id = batch_id;
    }

    return 0;
}


/* Free batch tracking records */
static void clear_batch_records(void)
{
    BatchRecord *current = counted_batches;

    while (current != NULL) {
        BatchRecord *next = current->next;
        free(current);
        current = next;
    }

    counted_batches = NULL;
    max_batch_id = 0;
}


/* Store final production quantities for a batch */
static void store_batch_quantities(int batch_id,
                                   float oil,
                                   float water,
                                   float pomace)
{
    BatchRecord *current = counted_batches;

    while (current != NULL) {

        if (current->batch_id == batch_id) {
            current->oil_quantity = oil;
            current->water_quantity = water;
            current->pomace_quantity = pomace;
            return;
        }

        current = current->next;
    }
}


/* Convert official StageID to GUI position */
static int stage_id_to_display_index(StageID stage_id)
{
    switch (stage_id) {
        case STAGE_RECEPTION:
            return 0;

        case STAGE_CLEANING:
            return 1;

        case STAGE_WASHING:
            return 2;

        case STAGE_SEPARATION:
            return 3;

        case STAGE_MALAXATION:
            return 4;

        case STAGE_CRUSHING:
            return 5;

        default:
            return -1;
    }
}


/* Convert stage status to readable text */
static const char *get_status_text(StageStatus status)
{
    switch (status) {
        case STAGE_IDLE:
            return "IDLE";

        case STAGE_RUNNING:
            return "RUNNING";

        case STAGE_COMPLETED:
            return "COMPLETED";

        case STAGE_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}


/*
 * Read FIFO data and assemble complete ProcessMessage records.
 *
 * This handles incomplete reads by keeping received bytes
 * until a complete ProcessMessage is available.
 */
static void poll_factory_messages(int value)
{
    (void)value;

    if (display_fifo_fd >= 0) {

        for (;;) {

            ssize_t n = read(
                display_fifo_fd,
                fifo_buffer + fifo_bytes_used,
                sizeof(ProcessMessage) - fifo_bytes_used
            );

            if (n > 0) {

                fifo_bytes_used += (size_t)n;

                if (fifo_bytes_used == sizeof(ProcessMessage)) {

                    ProcessMessage msg;

                    memcpy(
                        &msg,
                        fifo_buffer,
                        sizeof(ProcessMessage)
                    );

                    fifo_bytes_used = 0;

                    display_process_message(&msg);
                }

                continue;
            }

            if (n == 0) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            perror("[Display] FIFO read");
            break;
        }
    }

    if (!factory_finished) {
        glutTimerFunc(100, poll_factory_messages, 0);
    }
}


/* Draw a filled rectangle */
static void draw_rectangle(float x, float y,
                           float width, float height)
{
    glBegin(GL_QUADS);

    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);

    glEnd();
}


/* Draw text using FreeGLUT */
static void draw_text(float x, float y, const char *text)
{
    if (text == NULL) {
        return;
    }

    glRasterPos2f(x, y);

    while (*text != '\0') {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *text);
        text++;
    }
}


/* Draw arrow pointing right */
static void draw_arrow_right(float x, float y)
{
    glColor3f(0.9f, 0.9f, 0.9f);

    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x + 0.07f, y);
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2f(x + 0.07f, y);
    glVertex2f(x + 0.05f, y + 0.02f);
    glVertex2f(x + 0.05f, y - 0.02f);
    glEnd();
}


/* Draw arrow pointing left */
static void draw_arrow_left(float x, float y)
{
    glColor3f(0.9f, 0.9f, 0.9f);

    glBegin(GL_LINES);
    glVertex2f(x, y);
    glVertex2f(x - 0.07f, y);
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2f(x - 0.07f, y);
    glVertex2f(x - 0.05f, y + 0.02f);
    glVertex2f(x - 0.05f, y - 0.02f);
    glEnd();
}


/* Draw arrow pointing down */
static void draw_arrow_down(float x, float y_start,
                            float y_end)
{
    glColor3f(0.9f, 0.9f, 0.9f);

    glBegin(GL_LINES);
    glVertex2f(x, y_start);
    glVertex2f(x, y_end);
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2f(x, y_end);
    glVertex2f(x - 0.02f, y_end + 0.04f);
    glVertex2f(x + 0.02f, y_end + 0.04f);
    glEnd();
}


/* Set stage rectangle color according to its status */
static void set_stage_color(StageStatus status)
{
    switch (status) {
        case STAGE_IDLE:
            glColor3f(0.45f, 0.45f, 0.45f);
            break;

        case STAGE_RUNNING:
            glColor3f(0.95f, 0.75f, 0.15f);
            break;

        case STAGE_COMPLETED:
            glColor3f(0.20f, 0.65f, 0.35f);
            break;

        case STAGE_ERROR:
            glColor3f(0.85f, 0.20f, 0.20f);
            break;

        default:
            glColor3f(0.45f, 0.45f, 0.45f);
            break;
    }
}


/* Draw one statistic */
static void draw_statistic(float x, float y,
                           const char *label,
                           const char *value)
{
    glColor3f(0.70f, 0.78f, 0.85f);
    draw_text(x, y, label);

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(x, y - 0.07f, value);
}


/* Draw factory statistics panel */
static void draw_factory_statistics(void)
{
    char text[64];

    glColor3f(0.15f, 0.23f, 0.30f);
    draw_rectangle(-0.93f, -0.94f, 1.86f, 0.34f);

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(-0.87f, -0.66f, "FACTORY STATISTICS");

    snprintf(text, sizeof(text), "%d",
             factory_stats.total_batches);

    draw_statistic(-0.87f, -0.75f,
                   "Total Batches", text);

    snprintf(text, sizeof(text), "%d",
             factory_stats.accepted_batches);

    draw_statistic(-0.25f, -0.75f,
                   "Accepted", text);

    snprintf(text, sizeof(text), "%d",
             factory_stats.rejected_batches);

    draw_statistic(0.37f, -0.75f,
                   "Rejected", text);

    glColor3f(1.0f, 1.0f, 1.0f);

    snprintf(text, sizeof(text), "Oil: %.2f kg",
             factory_stats.total_oil);
    draw_text(-0.87f, -0.89f, text);

    snprintf(text, sizeof(text), "Water: %.2f kg",
             factory_stats.total_water);
    draw_text(-0.25f, -0.89f, text);

    snprintf(text, sizeof(text), "Pomace: %.2f kg",
             factory_stats.total_pomace);
    draw_text(0.37f, -0.89f, text);
}


/* Draw sensor monitoring panel */
static void draw_factory_sensors(void)
{
    char text[64];

    glColor3f(0.15f, 0.23f, 0.30f);
    draw_rectangle(-0.93f, -0.58f, 1.86f, 0.18f);

    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(-0.87f, -0.45f, "SENSOR MONITORING");

    snprintf(text, sizeof(text),
             "Temperature: %.1f C",
             factory_sensors.temperature);

    draw_text(-0.87f, -0.53f, text);

    snprintf(text, sizeof(text),
             "Pressure: %.1f",
             factory_sensors.pressure);

    draw_text(-0.25f, -0.53f, text);

    if (factory_sensors.alarm_active) {
        glColor3f(1.0f, 0.30f, 0.30f);
        draw_text(0.37f, -0.53f, "Status: ALARM");
    } else {
        glColor3f(0.30f, 0.90f, 0.45f);
        draw_text(0.37f, -0.53f, "Status: NORMAL");
    }
}


/* Update the current factory alert */
void display_update_alert(AlertType type, const char *message)
{
    if (type < ALERT_NONE || type > ALERT_ERROR) {
        return;
    }

    current_alert_type = type;

    if (message == NULL) {
        current_alert_message[0] = '\0';
    } else {
        snprintf(current_alert_message,
                 sizeof(current_alert_message),
                 "%s", message);
    }

    if (display_initialized) {
        glutPostRedisplay();
    }
}


/* Draw factory alerts panel */
static void draw_factory_alerts(void)
{
    glColor3f(0.15f, 0.23f, 0.30f);
    draw_rectangle(-0.93f, 0.52f, 1.86f, 0.12f);

    if (current_alert_type == ALERT_ERROR) {
        glColor3f(1.0f, 0.30f, 0.30f);
    } else if (current_alert_type == ALERT_WARNING) {
        glColor3f(1.0f, 0.80f, 0.20f);
    } else {
        glColor3f(0.80f, 0.90f, 1.0f);
    }

    const char *prefix = "INFO";

    if (current_alert_type == ALERT_ERROR) {
        prefix = "ERROR";
    } else if (current_alert_type == ALERT_WARNING) {
        prefix = "WARNING";
    } else if (current_alert_type == ALERT_NONE) {
        prefix = "STATUS";
    }

    char text[160];

    snprintf(text, sizeof(text), "%s: %.110s",
             prefix,
             current_alert_type == ALERT_NONE
                 ? "No alerts"
                 : current_alert_message);

    draw_text(-0.87f, 0.565f, text);
}


/* Main rendering function */
static void render(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0f, 1.0f, 1.0f);

    draw_text(-0.32f, 0.65f,
              "OLIVE OIL FACTORY SIMULATION");

    /* Draw production stages */
    for (int i = 0; i < NUM_PRODUCTION_STAGES; i++) {

        float x = -0.90f + (i % 3) * 0.65f;
        float y = 0.25f - (i / 3) * 0.60f;

        set_stage_color(stage_status[i]);

        draw_rectangle(x, y, 0.48f, 0.20f);

        glColor3f(1.0f, 1.0f, 1.0f);

        draw_text(x + 0.08f,
                  y + 0.13f,
                  stage_names[i]);

        draw_text(x + 0.08f,
                  y + 0.05f,
                  get_status_text(stage_status[i]));

        if (i < 2) {
            draw_arrow_right(x + 0.50f, y + 0.10f);
        }

        if (i == 2) {
            draw_arrow_down(x + 0.24f, y, y - 0.40f);
        }

        if (i == 4 || i == 5) {
            draw_arrow_left(x - 0.02f, y + 0.10f);
        }
    }

    draw_factory_alerts();
    draw_factory_sensors();
    draw_factory_statistics();

    glutSwapBuffers();
}


/* Update stage status using official StageID */
void display_update_stage(StageID stage_id, StageStatus status)
{
    int display_index = stage_id_to_display_index(stage_id);

    if (display_index < 0) {
        return;
    }

    if (status < STAGE_IDLE || status > STAGE_ERROR) {
        return;
    }

    stage_status[display_index] = status;

    if (display_initialized) {
        glutPostRedisplay();
    }
}


/* Update factory statistics */
void display_update_statistics(const FactoryStatistics *stats)
{
    if (stats == NULL) {
        return;
    }

    factory_stats = *stats;

    if (display_initialized) {
        glutPostRedisplay();
    }
}


/* Update sensor readings */
void display_update_sensors(const FactorySensors *sensors)
{
    if (sensors == NULL) {
        return;
    }

    factory_sensors = *sensors;

    if (display_initialized) {
        glutPostRedisplay();
    }
}


/* Close GUI when ESC is pressed */
static void keyboard(unsigned char key, int x, int y)
{
    (void)x;
    (void)y;

    if (key == 27) {
        printf("[Display] ESC pressed. Closing graphics window.\n");
        glutLeaveMainLoop();
    }
}


/* Initialize OpenGL display */
void display_init(int argc, char **argv)
{
    /* Reset all GUI data for a fresh run */
    factory_stats = (FactoryStatistics){0};
    factory_sensors = (FactorySensors){0};

    for (int i = 0; i < NUM_PRODUCTION_STAGES; i++) {
        stage_status[i] = STAGE_IDLE;
    }

    clear_batch_records();

    fifo_bytes_used = 0;
    factory_finished = 0;

    current_alert_type = ALERT_NONE;
    current_alert_message[0] = '\0';

    glutInit(&argc, argv);

    glutSetOption(
        GLUT_ACTION_ON_WINDOW_CLOSE,
        GLUT_ACTION_GLUTMAINLOOP_RETURNS
    );

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1000, 650);

    glutCreateWindow("Olive Oil Factory Simulation");

    glClearColor(0.10f, 0.15f, 0.20f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glutDisplayFunc(render);
    glutKeyboardFunc(keyboard);

    /*
     * Remove a stale FIFO from a previous run.
     * Do not run two factory instances simultaneously.
     */
    if (unlink(DISPLAY_FIFO) == -1 && errno != ENOENT) {
        perror("[Display] unlink old DISPLAY_FIFO");
    }

    if (mkfifo(DISPLAY_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("[Display] mkfifo DISPLAY_FIFO");
        return;
    }

    display_fifo_fd = open(DISPLAY_FIFO, O_RDWR | O_NONBLOCK);

    if (display_fifo_fd == -1) {
        perror("[Display] open DISPLAY_FIFO");
        return;
    }

    display_initialized = 1;

    printf("[Display] DISPLAY_FIFO is ready: %s\n", DISPLAY_FIFO);

    glutTimerFunc(100, poll_factory_messages, 0);
}


/* Start GUI event loop */
void display_run(void)
{
    glutMainLoop();
}


/* Release GUI resources */
void display_close(void)
{
    if (display_fifo_fd >= 0) {
        close(display_fifo_fd);
        display_fifo_fd = -1;
    }

    display_initialized = 0;

    fifo_bytes_used = 0;
    clear_batch_records();

    if (unlink(DISPLAY_FIFO) == -1 && errno != ENOENT) {
        perror("[Display] unlink DISPLAY_FIFO");
    }

    printf("[Display] Graphics resources released.\n");
}


/*
 * Validate final production quantities.
 *
 * An accepted production result must contain positive
 * oil, water and pomace quantities.
 *
 * Preliminary messages with zero quantities are ignored.
 */
static int valid_production_quantities(const ProcessMessage *msg)
{
    if (msg == NULL) {
        return 0;
    }

    if (!isfinite(msg->oil_quantity) ||
        !isfinite(msg->water_quantity) ||
        !isfinite(msg->pomace_quantity)) {
        return 0;
    }

    if (msg->oil_quantity <= 0.0f ||
        msg->water_quantity <= 0.0f ||
        msg->pomace_quantity <= 0.0f) {
        return 0;
    }

    return 1;
}


/* Process factory messages and update GUI */
void display_process_message(const ProcessMessage *msg)
{
    if (msg == NULL) {
        return;
    }

    /* Update sensor readings for production stages */
    if (msg->batch_id > 0 &&
        msg->stage_id >= STAGE_RECEPTION &&
        msg->stage_id <= STAGE_SEPARATION) {

        FactorySensors sensors = factory_sensors;

        sensors.temperature = msg->temperature;
        sensors.pressure = msg->pressure;
        sensors.alarm_active = 0;

        display_update_sensors(&sensors);
    }

    switch (msg->message_type) {

        case MSG_STAGE_STARTED:

            display_update_stage(
                (StageID)msg->stage_id,
                STAGE_RUNNING
            );

            break;


        case MSG_STAGE_COMPLETED:

            display_update_stage(
                (StageID)msg->stage_id,
                STAGE_COMPLETED
            );

            break;


        case MSG_SENSOR_UPDATE:

            break;


        /*
         * ACCEPTED BATCH
         *
         * Ignore preliminary accepted messages with zero quantities.
         * Only count a batch when its actual final production
         * quantities have arrived.
         */
        case MSG_BATCH_ACCEPTED:

            if (!valid_production_quantities(msg)) {
                break;
            }

            /* Ignore duplicate final results */
            if (batch_already_counted(msg->batch_id)) {
                break;
            }

            /* Update batch counters */
            factory_stats.total_batches++;
            factory_stats.accepted_batches++;

            /* Add final production quantities */
            factory_stats.total_oil += msg->oil_quantity;
            factory_stats.total_water += msg->water_quantity;
            factory_stats.total_pomace += msg->pomace_quantity;

            /* Store quantities for this individual batch */
            store_batch_quantities(
                msg->batch_id,
                msg->oil_quantity,
                msg->water_quantity,
                msg->pomace_quantity
            );

            printf("\n[Display] FINAL RESULT: Batch %d ACCEPTED\n",
                   msg->batch_id);

            printf("[DISPLAY] Batch %d accepted. GUI statistics updated.\n",
                   msg->batch_id);

            break;


        /*
         * REJECTED BATCH
         *
         * Count rejected batches once.
         * Do not add rejected quantities to production totals.
         */
        case MSG_BATCH_REJECTED:

            if (!batch_already_counted(msg->batch_id)) {

                factory_stats.total_batches++;
                factory_stats.rejected_batches++;

                printf("[Display] Batch %d REJECTED.\n",
                       msg->batch_id);

                printf("[Display] Rejected batch was not added "
                       "to production quantities.\n");
            }

            display_update_alert(
                ALERT_WARNING,
                msg->message
            );

            break;


        case MSG_ERROR:

            display_update_alert(
                ALERT_ERROR,
                msg->message
            );

            display_update_stage(
                (StageID)msg->stage_id,
                STAGE_ERROR
            );

            break;


        case MSG_WARNING:

            display_update_alert(
                ALERT_WARNING,
                msg->message
            );

            break;


        case MSG_INFO:

            display_update_alert(
                ALERT_INFO,
                msg->message
            );

            break;


        case MSG_FACTORY_SHUTDOWN:

            factory_finished = 1;

            if (msg->status == 0) {

                display_update_alert(
                    ALERT_INFO,
                    "FACTORY COMPLETED - Press ESC to close"
                );

                printf("\n");
                printf("============================================================\n");
                printf("                  FINAL PRODUCTION RESULTS\n");
                printf("============================================================\n");

            } else {

                display_update_alert(
                    ALERT_WARNING,
                    "EMERGENCY SHUTDOWN - Press ESC to close"
                );

                printf("\n");
                printf("============================================================\n");
                printf("                  FACTORY STOPPED\n");
                printf("============================================================\n");
            }

            printf("\nBATCH DETAILS\n");
            printf("------------------------------------------------------------\n");

            /* Display production quantities for each batch */
            for (int id = 1; id <= max_batch_id; id++) {

                BatchRecord *current = counted_batches;

                while (current != NULL && current->batch_id != id) {
                    current = current->next;
                }

                if (current != NULL) {

                    printf("\nBatch %d:\n", current->batch_id);

                    if (current->oil_quantity > 0.0f &&
                        current->water_quantity > 0.0f &&
                        current->pomace_quantity > 0.0f) {

                        printf("  Olive Oil : %.2f kg\n",
                               current->oil_quantity);

                        printf("  Water     : %.2f kg\n",
                               current->water_quantity);

                        printf("  Pomace    : %.2f kg\n",
                               current->pomace_quantity);

                    } else {

                        printf("  Rejected - No production quantities.\n");
                    }
                }
            }

            printf("\n============================================================\n");
            printf("                  TOTAL PRODUCTION\n");
            printf("============================================================\n");

            printf("Total Batches : %d\n", factory_stats.total_batches);
            printf("Accepted      : %d\n", factory_stats.accepted_batches);
            printf("Rejected      : %d\n", factory_stats.rejected_batches);

            printf("------------------------------------------------------------\n");

            printf("Olive Oil     : %.2f kg\n", factory_stats.total_oil);
            printf("Water         : %.2f kg\n", factory_stats.total_water);
            printf("Pomace        : %.2f kg\n", factory_stats.total_pomace);

            printf("============================================================\n\n");

            break;


        default:

            break;
    }

    if (display_initialized) {
        glutPostRedisplay();
    }
}
