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
        - Uses a thread pool to receive packages from package.c via a message queue
        - Calculates dot product subtasks from the packages (which consist of parts of matrices)
        - Sends completed calculations to package.c via the message queue
        - Runs on a loop until terminated by SIGQUIT (ctl-\)
        - Signals SIGINT (ctl-c) for message queue updates

    RESOURCES NEEDED:
        - this source file (compute.c)
        - package.c source file, opened and running in a separate terminal
        - proj.h header file
        - makefile
        - the .dat files that go along with package.c

    COMMAND:
    make compute
    ./compute  <thread pool size>  -n

    -n IS OPTIONAL:
        - If used, program will print each calculated matrix cell
        - If not used, program will instead print confirmation of calculations being sent to message queue

    SOURCES are listed in package.c source file
*/

key_t KEY;
pthread_mutex_t sLock, rLock; //needed for updating sent and received integers used by the signal handler
int sent, received; //needed for correct output when signaled

//SIGINT signal handler
void handle(int sig) {
    printf("\nJobs Sent %d Jobs Received %d\n", sent, received);
}

//receive packages from and send calculations to package.c via a message queue
void *compute(void *tArgs) {
    
    //declare / initialize variables
    struct ThreadArgs2 *args = tArgs;
    assert(args != NULL);
    int msgid = args->msgid;
    int n = args->n;
    int rc, size;

    //threads continue in an infinite loop until program is terminated by SIGQUIT
    while (1) {

        //declare message structs
        struct Msg msg1, msg2;

        //receive package from message queue
        if (msgrcv(msgid, &msg1, 416, 1, 0) == -1) {
            fprintf(stderr, "Could not receive message type 1\n");
            return NULL;
        }
        size = 16 + (4 * (msg1.innerDim * 2));
        printf("Receiving job id %d type 1 size %d\n", msg1.jobid, size);
        pthread_mutex_lock(&rLock);
        received++;
        pthread_mutex_unlock(&rLock);

        //compute dot product subtask
        int i, j, sum = 0;
        for (i = 0; i < (msg1.innerDim * 2); i++) {
            j = i + 1;
            sum = sum + (msg1.data[i] * msg1.data[j]);
            i++;
        }

        //populate new message with completed calculation
        msg2.type = 2;
        msg2.jobid = msg1.jobid;
        msg2.rowvec = msg1.rowvec;
        msg2.colvec = msg1.colvec;
        msg2.innerDim = msg1.innerDim;
        msg2.data[0] = sum;

        //send calculation to message queue
        if ((rc = msgsnd(msgid, &msg2, 20, 0)) == -1) {
            fprintf(stderr, "Could not send message #%d\n", msg2.jobid);
            return NULL;
        }
        if (n == 1) {
            printf("Sum for cell %d,%d is %d\n", msg2.rowvec, msg2.colvec, sum);
        }
        else {
            printf("Sending job id %d type 2 size 20 (rc=%d)\n", msg2.jobid, rc);
        }
        pthread_mutex_lock(&sLock);
        sent++;
        pthread_mutex_unlock(&sLock);

    }

    return NULL;
}

//create thread pool
int main(int argc, char *argv[]) {

    //signal handler
    sent = 0, received = 0;
    signal(SIGINT, handle);

    //check for command line errors
    if ((argc != 2) && (argc != 3)) {
        fprintf(stderr, "Wrong number of command line arguments\n");
        return -1;
    }
    if (isdigit(*argv[1]) == 0) {
        fprintf(stderr, "Command line argument for thread pool size was not an integer\n");
        return -1;
    }
    int n = 0;
    if ((argc == 3) && (strcmp(argv[2], "-n") == 0)) {
        n = 1;
    }

    //initialize mutexes
    if (pthread_mutex_init(&rLock, NULL) != 0) {
        fprintf(stderr, "Could not initialize rLock\n");
        return -1;
    }
    if (pthread_mutex_init(&sLock, NULL) != 0) {
        fprintf(stderr, "Could not initialize sLock\n");
        return -1;
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
    struct ThreadArgs2 tArgs;
    tArgs.msgid = msgid;
    tArgs.n = n;
    assert(tArgs.msgid != -1);
    assert((tArgs.n == 0) || (tArgs.n == 1));

    //create pool of threads and send them to compute function
    int pool = atoi(argv[1]);
    pthread_t threads[pool];
    int j;
    for (j = 0; j < pool; j++) {
        if (pthread_create(&threads[j], NULL, compute, &tArgs) != 0) {
            fprintf(stderr, "Thread #%d was not created\n", j);
            return -1;
        }
    }

    //join threads as a precaution (they shouldn't ever make it this far)
    for (j = 0; j < pool; j++) {
        if (pthread_join(threads[j], NULL) != 0) {
            fprintf(stderr, "Cannot join thread #%d\n", j);
            return -1;
        }
    }

    return 0;
}