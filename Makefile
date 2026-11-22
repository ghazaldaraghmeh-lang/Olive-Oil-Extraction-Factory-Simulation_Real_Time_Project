
CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -fopenmp -Iinclude

LDFLAGS = -fopenmp -lm -lglut -lGLU -lGL

TARGET = Olive_Factory

SRC = \
    core/main.c \
    core/process_manager.c \
    core/supervisor.c \
    graphics/display.c \
    monitoring/config.c \
    monitoring/logger.c \
    monitoring/quality_control.c \
    monitoring/sensors.c \
    production_input/reception.c \
    production_input/cleaning.c \
    production_input/washing.c \
    production_output/crushing.c \
    production_output/malaxation.c \
    production_output/parallel_processing.c \
    production_output/separation.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) factory.log
	rm -f /tmp/olive_factory_status
	rm -f /tmp/olive_factory_display

run: $(TARGET)
	./$(TARGET) config.txt

.PHONY: all clean run
