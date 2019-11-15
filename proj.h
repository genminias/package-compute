#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

///////////////////////////////////////////////////////////////////////////////////////////

/*
    GENEVIEVE MINIAS
    CS300 - Nov 17, 2019

    Header file for package.c and compute.c source files
*/

///////////////////////////////////////////////////////////////////////////////////////////

// message struct for message queue
#ifndef QUEUE
#define QUEUE
typedef struct Msg {
    long type;
    int jobid;
    int rowvec;
    int colvec;
    int innerDim;
    int data[100];
} Msg;
#endif

///////////////////////////////////////////////////////////////////////////////////////////

// matrix struct including rows, columns, and a 2D array for the data
#ifndef MATRIX
#define MATRIX
typedef struct Matrix {
    int rows;
    int cols;
    int **data;
} Matrix;
#endif

///////////////////////////////////////////////////////////////////////////////////////////

// thread arguments struct to hold info needed for package function
#ifndef ARGUMENTS1
#define ARGUMENTS1
typedef struct ThreadArgs1 {
    struct Matrix *matrix1;
    struct Matrix *matrix2;
    int msgid;
} ThreadArgs1;
#endif

// thread arguments struct to hold info needed for compute function
#ifndef ARGUMENTS2
#define ARGUMENTS2
typedef struct ThreadArgs2 {
    int msgid;
    int n;
} ThreadArgs2;
#endif

///////////////////////////////////////////////////////////////////////////////////////////