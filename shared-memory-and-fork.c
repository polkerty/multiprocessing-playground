#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_processes>\n", argv[0]);
        return 1;
    }

    int num_processes = atoi(argv[1]);
    if (num_processes <= 0) {
        fprintf(stderr, "Error: Number of processes must be positive.\n");
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

    *(int*) shared_mem = 1;
    ((int*) shared_mem)[1] = 1;

    // Fork into num_processes
    for (int i = 0; i < num_processes; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        } else if (pid == 0) {
            // Child process
            int n1 = *(int*)shared_mem;
            int n2 = ((int*)shared_mem)[1];
            printf("Child %d reading: '%d + %d'\n", i, n1, n2);

            // Write something back to the shared memory
            int n3 = n1 + n2;
            *(int*)shared_mem = n2;
            ((int*)shared_mem)[1] = n3;

            printf("Child %d writing: '%d %d'\n", i, n2, n3);

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
    printf("Parent reading final content: '%d'\n", *(int*)shared_mem);

    // Detach from shared memory in the parent
    shmdt(shared_mem);

    // Remove the shared memory segment
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
