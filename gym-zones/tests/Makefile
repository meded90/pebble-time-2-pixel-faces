CC := clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2

TARGETS := test_fitness_math test_workout_logic
INCLUDES := -I../src/c

.PHONY: all test sanitize clean

all: $(TARGETS)

test_fitness_math: test_fitness_math.c ../src/c/fitness_math.c ../src/c/fitness_math.h
	$(CC) $(CFLAGS) $(INCLUDES) test_fitness_math.c ../src/c/fitness_math.c -o $@

test_workout_logic: test_workout_logic.c ../src/c/workout_logic.c ../src/c/workout_logic.h
	$(CC) $(CFLAGS) $(INCLUDES) test_workout_logic.c ../src/c/workout_logic.c -o $@

test: $(TARGETS)
	./test_fitness_math
	./test_workout_logic
	node test_package.js

sanitize: clean
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(INCLUDES) test_fitness_math.c ../src/c/fitness_math.c -o test_fitness_math
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(INCLUDES) test_workout_logic.c ../src/c/workout_logic.c -o test_workout_logic
	./test_fitness_math
	./test_workout_logic

clean:
	rm -f $(TARGETS)
	rm -rf $(addsuffix .dSYM,$(TARGETS))
