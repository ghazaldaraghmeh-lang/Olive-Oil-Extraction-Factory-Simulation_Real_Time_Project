
#include <stdio.h>
#include <stdlib.h>
#include <GL/freeglut.h>
#include "display.h"

/*
 * NUM_PRODUCTION_STAGES is defined in project_contract.h
 * and included through display.h.
 */

/* Stage names in display order */
static const char *stage_names[NUM_PRODUCTION_STAGES] = {
    "Reception",
    "Cleaning",
    "Washing",
    "Separation",
    "Malaxation",
    "Crushing"
};

/* Temporary stage statuses for GUI testing */
static StageStatus stage_status[NUM_PRODUCTION_STAGES] = {
    STAGE_COMPLETED,
    STAGE_COMPLETED,
    STAGE_RUNNING,
    STAGE_IDLE,
    STAGE_IDLE,
    STAGE_IDLE
};

/* Factory data */
static FactoryStatistics factory_stats = {0};
static FactorySensors factory_sensors = {0};

static int display_initialized = 0;


/* Factory alerts */
static AlertType current_alert_type = ALERT_NONE;
static char current_alert_message[MAX_MESSAGE] = "";


/*
 * Convert official StageID to its position in GUI.
 *
 * Reception  -> Cleaning   -> Washing
 * Separation <- Malaxation <- Crushing
 */
static int stage_id_to_display_index(StageID stage_id)
{
    switch (stage_id) {
        case STAGE_RECEPTION:
            return 0;

        case STAGE_CLEANING:
            return 1;

        case STAGE_WASHING:
            return 2;

        case STAGE_CRUSHING:
            return 5;

        case STAGE_MALAXATION:
            return 4;

        case STAGE_SEPARATION:
            return 3;

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

/* Set stage rectangle color */
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

        draw_text(0.37f, -0.53f,
                  "Status: ALARM");
    } else {
        glColor3f(0.30f, 0.90f, 0.45f);

        draw_text(0.37f, -0.53f,
                  "Status: NORMAL");
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

/* Draw factory alerts */
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

    /* Factory title */
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

        /* First row arrows */
        if (i < 2) {
            draw_arrow_right(x + 0.50f,
                             y + 0.10f);
        }

        /* Washing to Crushing */
        if (i == 2) {
            draw_arrow_down(x + 0.24f,
                            y,
                            y - 0.40f);
        }

        /* Second row arrows */
        if (i == 4 || i == 5) {
            draw_arrow_left(x - 0.02f,
                            y + 0.10f);
        }
    }

    /* Draw monitoring panels */
    draw_factory_alerts();
    draw_factory_sensors();
    draw_factory_statistics();

    glutSwapBuffers();
}

/* Update stage status using official StageID */
void display_update_stage(StageID stage_id, StageStatus status)
{
    int display_index = stage_id_to_display_index(stage_id);

    /* Ignore unsupported stages */
    if (display_index == -1) {
        return;
    }

    /* Validate status */
    if (status < STAGE_IDLE || status > STAGE_ERROR) {
        return;
    }

    /* Update correct stage */
    stage_status[display_index] = status;

    /* Request redraw */
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

/* Initialize OpenGL display */
void display_init(int argc, char **argv)
{
    glutInit(&argc, argv);

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

    display_initialized = 1;
}

/* Start GUI event loop */
void display_run(void)
{
    glutMainLoop();
}

/* Close GUI event loop */
void display_close(void)
{
    if (display_initialized) {
        glutLeaveMainLoop();
        display_initialized = 0;
    }
}

/* Process factory messages and update the GUI */
void display_process_message(const ProcessMessage *msg)
{
    if (msg == NULL) {
        return;
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
        {
            FactorySensors sensors = factory_sensors;

            sensors.temperature = msg->temperature;
            sensors.pressure = msg->pressure;

            /* Preserve the current alarm state until
               the supervisor defines alarm semantics. */
            display_update_sensors(&sensors);
            break;
        }

        case MSG_ERROR:
            display_update_alert(ALERT_ERROR, msg->message);
            break;

        case MSG_WARNING:
            display_update_alert(ALERT_WARNING, msg->message);
            break;

        case MSG_INFO:
            display_update_alert(ALERT_INFO, msg->message);
            break;


        default:
            /* Other message types will be handled later */
            break;
    }
}
