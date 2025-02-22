#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdint.h>
#include <semaphore.h>
#include <dispatch/dispatch.h>

#define INT_NAME(i)  ('A' + ((i) / (26 * 26))),  ('a' + (((i) % (26 * 26))/26)), ('a' + ((i) % 26)) 

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_processes> <vars per process>\n", argv[0]);
        return 1;
    }

    int num_processes = atoi(argv[1]);
    if (num_processes <= 0) {
        fprintf(stderr, "Error: Number of processes must be positive.\n");
        return 1;
    }

    int num_vars = atoi(argv[2]);
    if (num_vars <= 1) {
        fprintf(stderr, "Error: Number of vars must be 2 or more.\n");
        return 1;
    }

    // Key for the shared memory segment (could also use ftok for a real file-based key)
    key_t key = 1234;

    // Create the shared memory segment, size = 1024 bytes
    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);

    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    // Attach the shared memory segment to our process
    char *shared_mem = (char *)shmat(shmid, NULL, 0);
    if (shared_mem == (char *)-1) {
        perror("shmat");
        return 1;
    }

    for ( int i = 0; i < num_vars; ++i) {
        ((unsigned int*) shared_mem)[i] = 1;
    }

    // put a semaphore just at the end of the integers
    dispatch_semaphore_t *sem = (dispatch_semaphore_t *)(&((unsigned int*)shared_mem)[num_vars]);

    *sem = dispatch_semaphore_create(1); 

    // Fork into num_processes
    for (int i = 0; i < num_processes; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        } else if (pid == 0) {
            unsigned int vals[num_vars];

            unsigned int highest = 0;

            dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);

            for ( int j = 0; j < num_vars; ++j ) {
                unsigned int val =((unsigned int*) shared_mem)[j];
                printf("%c%c%c %d < [%d] = %u\n", INT_NAME(i), i, j, val );
                vals[j] = val;
                if ( val > highest ) {
                    // Not necessarily true due to integer overflow!
                    highest = val;
                }
            }

            printf("^%c%c%c %d = %u\n", INT_NAME(i), i, highest);

            for ( int j = 0; j < num_vars; ++j ) {
                unsigned int next = vals[j] + 1;
                ((unsigned int*) shared_mem)[j] = next;
                printf("%c%c%c %d > [%d] = %u\n", INT_NAME(i), i, j, next);
            }

            dispatch_semaphore_signal(*sem);

            printf("--------------\n");


            // Detach from shared memory in the child
            shmdt(shared_mem);

            // Exit child
            exit(0);
        }
    }

    // Parent waits for all children to complete
    for (int i = 0; i < num_processes; i++) {
        wait(NULL);
    }

    // Now the parent reads the final contents in shared memory
    for ( int i = 0; i < num_vars; ++i) {
        printf("[%d] = %u\n", i, ((int*)shared_mem)[i]);
    }

    dispatch_release(*sem);

    // Detach from shared memory in the parent
    shmdt(shared_mem);

    // Remove the shared memory segment
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
