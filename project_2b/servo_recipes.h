/*
 *    Contains recipe declarations for demos.
 */

#ifndef SERVO_RECIPE_H
#define SERVO_RECIPE_H

// Define all of the commands that are valid
#define MOV         (0x20)
#define WAIT        (0x40)
#define LOOP        (0x80)
#define END_LOOP    (0xA0)
#define SKIP        (0x60)
#define JUMP        (0xC0) // TODO
#define RECIPE_END 	(0)

#define NUM_SERVO_POS (6)

extern int servo1_positions[6];
extern int servo2_positions[6];

/******        Simple Recipes        ******/
extern unsigned char recipe_verify_moves[];
extern unsigned char recipe_verify_moves_reverse[];

extern unsigned char recipe1[];

extern unsigned char recipe2[];

/******        RECIPE_END test        ******/
extern unsigned char recipe_continue[];

/******        Demo recipe        ******/
extern unsigned char recipe_demo[];

/******        Nested loop error recipe        ******/
extern unsigned char recipe_error_loop[];

/******        Command error recipe        ******/
extern unsigned char recipe_error_mov[];

/******        Skip command recipe        ******/
extern unsigned char recipe_skip[];
extern unsigned char recipe_skip_error[];

/******        Jump command recipe        ******/
extern unsigned char recipe_jump[];
extern unsigned char recipe_jump_error[];

#endif
