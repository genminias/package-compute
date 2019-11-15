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
#include "proj.h"

/*  
    GENEVIEVE MINIAS
    CS300 - Nov 17, 2019

    THIS PROGRAM:
        - Reads in two files that each contain a matrix
        - Uses threads to package up dot product subtasks (one row * one column)
        - Sends these packages on a message queue to compute.c
        - Receives the completed calculations for the output matrix from compute.c via the message queue
        - Prints the output matrix to stdout and to a file
        - Signals SIGINT (ctl-c) for message queue updates

    RESOURCES NEEDED:
        - this source file (package.c)
        - compute.c source file, opened and running in a separate terminal
        - proj.h header file
        - makefile
        - gaminias text file (for message queue key)
        - two input .dat files, each containing a matrix:
            - rows
            - columns
            - matrix data
        - the name for an output .dat file (this program will create it for you)

    COMMAND:
    make package
    ./package  <input matrix file 1>  <input matrix file 2>  <output matrix file>  <optional: secs between thread creation>

    SOURCES:
        - Matrix multiplication in C:
            https://www.programmingsimplified.com/c-program-multiply-matrices
        - Matrix structs:
            https://stackoverflow.com/questions/41128608/c-matrix-struct
            https://www.geeksforgeeks.org/dynamically-allocate-2d-array-c/
        - Message queues:
            https://www.tutorialspoint.com/inter_process_communication/inter_process_communication_message_queues.htm
            https://www.geeksforgeeks.org/ipc-using-message-queues/
            https://support.sas.com/documentation/onlinedoc/sasc/doc700/html/lr2/z2101550.htm
        - Signals:
            https://www.usna.edu/Users/cs/aviv/classes/ic221/s16/lec/19/lec.html#orgheadline13
        - Threads:
            https://stackoverflow.com/questions/1352749/multiple-arguments-to-function-called-by-pthread-create
            https://ubuntuforums.org/showthread.php?t=2168398
            https://stackoverflow.com/questions/12887214/pthread-exit-issues-returning-struct
            https://stackoverflow.com/questions/34487312/return-argument-struct-from-a-thread
        - Man pages
        - CS300 txbk (Operating Systems: Three Easy Pieces, Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau)
*/

key_t KEY;
pthread_mutex_t pLock, rLock; //needed for protection from a race condition while packaging messages and for updating sent and received integers used by the signal handler
int x, y, z, jobCount, dataCount; //needed for packaging messages correctly
int sent, received; //needed for correct output when signaled

//SIGINT signal handler
void handle(int sig) {
    printf("\nJobs Sent %d Jobs Received %d\n", sent, received);
}

//threads package up dot product subtasks, send packages to and receive calculations from compute.c via a message queue, and return the completed calculations to main
void *package(void *tArgs) {

    //declare / initialize variables
    struct ThreadArgs1 *args = tArgs;
    assert(args != NULL);
    int msgid = args->msgid;
    struct Msg msg, output;
    int rc, size, stop = 0;

    //lock critical section, create package, and send package to message queue
    pthread_mutex_lock(&pLock);
    while ((x < args->matrix1->rows) && (stop == 0)) {
        while ((y < args->matrix2->cols) && (stop == 0)) {

            //create package
            msg.type = 1;
            msg.jobid = jobCount;
            msg.rowvec = x;
            msg.colvec = y;
            msg.innerDim = args->matrix1->cols;
            while (z < args->matrix1->cols) {
                msg.data[dataCount] = args->matrix1->data[x][z];
                dataCount++;
                msg.data[dataCount] = args->matrix2->data[z][y];
                dataCount++;
                z++;
            }

            //send package to message queue 
            size = 16 + (4 * dataCount);
            if ((rc = msgsnd(msgid, &msg, size, 0)) == -1) {
                fprintf(stderr, "Could not send message #%d\n", msg.jobid);
                return NULL;
            }
            printf("Sending job id %d type 1 size %d (rc=%d)\n", msg.jobid, size, rc);
            sent++;

            jobCount++;
            dataCount = 0;
            y++;
            z = 0;
            stop = 1;
        }
        if (stop == 1) break;
        x++;
        y = 0;
    }
    stop = 0;
    pthread_mutex_unlock(&pLock);

    //receive calculation from message queue
    if (msgrcv(msgid, &output, 20, 2, 0) == -1) {
        fprintf(stderr, "Could not receive message type 2\n");
        return NULL;
    }
    printf("Receiving job id %d type 2 size 20\n", output.jobid);
    pthread_mutex_lock(&rLock);
    received++;
    pthread_mutex_unlock(&rLock);

    //allocate space on heap for calculation to be sent back to main function
    struct Msg *retval = malloc(sizeof(Msg));
    *retval = output;
    assert(retval != NULL);

    pthread_exit(retval);
}

//read file into matrix struct
struct Matrix *initMatrix(FILE *m) {
    int i, a, b;
    struct Matrix *matrix = malloc(sizeof(Matrix));
    fscanf(m, "%d %d ", &matrix->rows, &matrix->cols);
    assert((matrix->rows > 0) && (matrix->cols > 0));
    matrix->data = malloc(matrix->rows * sizeof(int *));
    for (i = 0; i < matrix->rows; i++) {
        matrix->data[i] = malloc(matrix->cols * sizeof(int));
    }
    for (a = 0; a < matrix->rows; a++) {
        for (b = 0; b < matrix->cols; b++) {
            fscanf(m, "%d ", &matrix->data[a][b]);
        }
    }
    return matrix;
}

//create input matrices, threads, and output matrix
int main(int argc, char *argv[]) {

    //signal handler
    sent = 0, received = 0;
    signal(SIGINT, handle);

    //check for command line errors
    if ((argc != 4) && (argc != 5)) {
        fprintf(stderr, "Wrong number of command line arguments\n");
        return -1;
    }
    if ((argc == 5) && (isdigit(*argv[4]) == 0)) {
        fprintf(stderr, "Command line argument for sleep time was not an integer\n");
        return -1;
    }
    int wait = 0;
    if (argc == 5) {
        wait = atoi(argv[4]);
    }

    //initialize mutexes
    if (pthread_mutex_init(&pLock, NULL) != 0) {
        fprintf(stderr, "Could not initialize pLock\n");
        return -1;
    }
    if (pthread_mutex_init(&rLock, NULL) != 0) {
        fprintf(stderr, "Could not initialize rLock\n");
        return -1;
    }

    //read first file into matrix struct
    FILE *firstMatrix = fopen(argv[1], "r");
    struct Matrix *matrix1 = initMatrix(firstMatrix);
    fclose(firstMatrix);
    assert(matrix1 != NULL);

    //read second file into matrix struct
    FILE *secondMatrix = fopen(argv[2], "r");
    struct Matrix *matrix2 = initMatrix(secondMatrix);
    fclose(secondMatrix);
    assert(matrix2 != NULL);

    //check if matrices are compatible (even though already assumed)
    if (matrix1->cols != matrix2->rows) {
        fprintf(stderr, "Input matrices are not compatible\n");
        return -1;
    }

    //allocate memory for output matrix
    struct Matrix *outMatrix = malloc(sizeof(Matrix));
    outMatrix->rows = matrix1->rows;
    outMatrix->cols = matrix2->cols;
    assert((outMatrix->rows > 0) && (outMatrix->cols > 0));
    outMatrix->data = malloc(outMatrix->rows * sizeof(int *));
    int k;
    for (k = 0; k < outMatrix->rows; k++) {
        outMatrix->data[k] = malloc(outMatrix->cols * sizeof(int));
    }

    //get message queue key
    if ((KEY = ftok("gaminias", 420)) == -1) {
        fprintf(stderr, "Message queue key could not be created\n");
        return -1;
    }

    //create or connect to message queue
    int msgid;
    if ((msgid = msgget(KEY, 0666)) != -1) {
        //do nothing, we good
    }
    else if ((msgid = msgget(KEY, 0666 | IPC_CREAT)) == -1) {
        fprintf(stderr, "Message queue could not be created or connected to\n");
        return -1;
    }

    //populate thread arguments struct
    struct ThreadArgs1 tArgs;
    tArgs.matrix1 = matrix1;
    tArgs.matrix2 = matrix2;
    tArgs.msgid = msgid;
    assert(tArgs.matrix1 != NULL);
    assert(tArgs.matrix2 != NULL);
    assert(tArgs.msgid != -1);

    //create threads (one per dot product subtask) and send them to package function
    x = 0, y = 0, z = 0, jobCount = 0, dataCount = 0;
    int tCount = matrix1->rows * matrix2->cols;
    pthread_t threads[tCount];
    int j;
    for (j = 0; j < tCount; j++) {
        if (pthread_create(&threads[j], NULL, package, &tArgs) != 0) {
            fprintf(stderr, "Thread #%d was not created\n", j);
            return -1;
        }
        if (argc == 5) {
            sleep(wait);
        }
    }

    //join threads and populate output matrix
    for (j = 0; j < tCount; j++) {
        void *msg;
        if (pthread_join(threads[j], &msg) != 0) {
            fprintf(stderr, "Cannot join thread #%d\n", j);
            return -1;
        }
        struct Msg *out = msg;
        assert(out != NULL);
        outMatrix->data[out->rowvec][out->colvec] = out->data[0];
        free(msg);
    }

    //print output matrix to stdout
    printf("\n");
    int m, n;
    for (m = 0; m < outMatrix->rows; m++) {
        for (n = 0; n < outMatrix->cols; n++) {
            printf("%d ", outMatrix->data[m][n]);
        }
        printf("\n");
    }
    printf("\n");

    //print output matrix to file
    FILE *outputMatrix = fopen(argv[3], "w");
    for (m = 0; m < outMatrix->rows; m++) {
        for (n = 0; n < outMatrix->cols; n++) {
            fprintf(outputMatrix, "%d ", outMatrix->data[m][n]);
        }
    }
    fclose(outputMatrix);

    //free matrix structs
    int i;
    for (i = 0; i < matrix1->rows; i++) {
        free(matrix1->data[i]);
    }
    free(matrix1->data);
    for (i = 0; i < matrix2->rows; i++) {
        free(matrix2->data[i]);
    }
    free(matrix2->data);
    for (i = 0; i < outMatrix->rows; i++) {
        free(outMatrix->data[i]);
    }
    free(outMatrix->data);
    free(matrix1);
    free(matrix2);
    free(outMatrix);

    return 0;
}