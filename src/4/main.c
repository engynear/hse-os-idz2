#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/fcntl.h>

#define SMOKER_1_SEM_NAME "/smoker_1"
#define SMOKER_2_SEM_NAME "/smoker_2"
#define SMOKER_3_SEM_NAME "/smoker_3"
#define QUEUE_SEM_NAME "/queue"
#define FOR_SMOKER_1_SEM_NAME "/for_smoker_1"
#define FOR_SMOKER_2_SEM_NAME "/for_smoker_2"
#define FOR_SMOKER_3_SEM_NAME "/for_smoker_3"
#define SHM_NAME "/customer_count"

sem_t *smoker_1_sem, *smoker_2_sem, *smoker_3_sem, *queue_sem, *for_smoker_1_sem, *for_smoker_2_sem, *for_smoker_3_sem;
int *smoker_count_ptr;
int shm_fd;

void smoker_behaviour(int id);
void cleanup();
void signal_handler(int nsig);
void initialize_semaphores();
void create_shared_memory();

void mediator_behaviour(int n);

void cleanup()
{
    sem_close(smoker_1_sem);
    sem_close(smoker_2_sem);
    sem_close(smoker_3_sem);
    sem_close(queue_sem);
    sem_close(for_smoker_1_sem);
    sem_close(for_smoker_2_sem);
    sem_close(for_smoker_3_sem);
    sem_unlink(SMOKER_1_SEM_NAME);
    sem_unlink(SMOKER_2_SEM_NAME);
    sem_unlink(SMOKER_3_SEM_NAME);
    sem_unlink(QUEUE_SEM_NAME);
    sem_unlink(FOR_SMOKER_1_SEM_NAME);
    sem_unlink(FOR_SMOKER_2_SEM_NAME);
    sem_unlink(FOR_SMOKER_3_SEM_NAME);
    munmap(smoker_count_ptr, sizeof(int));
    close(shm_fd);
    shm_unlink(SHM_NAME);
}

void signal_handler(int sig)
{
    cleanup();
    exit(0);
}

void initialize_semaphores()
{
    if ((smoker_1_sem = sem_open(SMOKER_1_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if ((smoker_2_sem = sem_open(SMOKER_2_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if ((smoker_3_sem = sem_open(SMOKER_3_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if ((queue_sem = sem_open(QUEUE_SEM_NAME, O_CREAT, 0600, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if ((for_smoker_1_sem = sem_open(FOR_SMOKER_1_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if ((for_smoker_2_sem = sem_open(FOR_SMOKER_2_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if ((for_smoker_3_sem = sem_open(FOR_SMOKER_3_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
}

void create_shared_memory()
{
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, 4) == -1)
    {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    smoker_count_ptr = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (smoker_count_ptr == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    char *p;

    cleanup();
    initialize_semaphores();
    create_shared_memory();

    if (argc != 2)
    {
        printf("Usage: %s <raund_count>\n", argv[0]);
        exit(-1);
    }

    int n = strtol(argv[1], &p, 10);
    smoker_count_ptr[0] = n;

    pid_t smoker_1 = fork();

    if (smoker_1 < 0)
    {
        perror("Can\'t fork smoker_1 1");
        exit(-1);
    }
    else if (smoker_1 == 0)
    {
        smoker_behaviour(1);
        exit(0);
    }
    else
    {
        pid_t smoker_2 = fork();

        if (smoker_2 == -1)
        {
            perror("Can\'t fork smoker_1 2");
            exit(-1);
        }
        else if (smoker_2 == 0)
        {
            smoker_behaviour(2);
            exit(0);
        }
        else
        {
            pid_t smoker_3 = fork();
            if (smoker_3 == -1)
            {
                perror("Can\'t fork smoker_1 2");
                exit(-1);
            }
            else if (smoker_3 == 0)
            {
                smoker_behaviour(3);
                exit(0);
            }
            else
            {
                pid_t mediator = fork();
                if (mediator == -1)
                {
                    perror("Can\'t fork mediator");
                    exit(-1);
                }
                else if (mediator == 0)
                {
                    mediator_behaviour(n);
                    exit(0);
                }

                for (int i = 1; i <= 4; i++)
                {
                    wait(0);
                }

                cleanup();
                printf("Все раунды завершены.\n");
                exit(0);
            }
        }
    }
}

void mediator_behaviour(int n)
{
    srand(time(NULL));

    while (n > 0)
    {
        int random_number = rand() % 3 + 1;
        if (random_number == 1)
        {
            fflush(stdout);
            printf("Посредник положил на стол бумагу и спички\n");
            sem_post(smoker_1_sem);
            sem_wait(for_smoker_1_sem);
            fflush(stdout);
            printf("Раунд пройден.\n");
            sleep(1);
        }
        else if (random_number == 2)
        {
            fflush(stdout);
            printf("Посредник положил на стол табак и спички\n");
            sem_post(smoker_2_sem);
            sem_wait(for_smoker_2_sem);
            fflush(stdout);
            printf("Раунд пройден.\n");
        }
        else
        {
            fflush(stdout);
            printf("Посредник положил на стол табак и бумагу\n");
            sem_post(smoker_3_sem);
            sem_wait(for_smoker_3_sem);
            fflush(stdout);
            printf("Раунд пройден.\n");
        }
        n--;
    }
    exit(0);
}

void smoker_behaviour(int id)
{
    fflush(stdout);
    printf("Курильщик номер %d готов, у него есть табак.\n", id);

    while (smoker_count_ptr[0] > 0)
    {
        if (id == 1)
        {
            sleep(1);
            if (smoker_count_ptr[0] == 0)
            {
                exit(0);
            }

            sem_wait(smoker_1_sem);
            if (smoker_count_ptr[0] == 0)
            {
                exit(0);
            }
            fflush(stdout);
            printf("Курильщик номер %d забирает со стола бумагу и спички и скуривает сигарету.\n", id);
            sleep(1);
            smoker_count_ptr[0]--;
            if (smoker_count_ptr[0] == 0)
            {
                sem_post(smoker_2_sem);
                sem_post(smoker_3_sem);
            }
            sem_post(for_smoker_1_sem);
        }
        else if (id == 2)
        {
            sleep(1);
            if (smoker_count_ptr[0] == 0)
            {
                exit(0);
            }

            sem_wait(smoker_2_sem);
            if (smoker_count_ptr[0] == 0)
            {
                exit(0);
            }
            fflush(stdout);
            printf("Курильщик номер %d забирает со стола табак и спички и скуривает сигарету.\n", id);
            sleep(1);
            smoker_count_ptr[0]--;
            if (smoker_count_ptr[0] == 0)
            {
                sem_post(smoker_1_sem);
                sem_post(smoker_3_sem);
            }
            sem_post(for_smoker_2_sem);
        }
        else
        {
            sleep(1);
            if (smoker_count_ptr[0] == 0)
            {
                exit(0);
            }

            sem_wait(smoker_3_sem);
            if (smoker_count_ptr[0] == 0)
            {
                exit(0);
            }
            fflush(stdout);
            printf("Курильщик номер %d забирает со стола табак и бумагу и скуривает сигарету.\n", id);
            sleep(1);
            smoker_count_ptr[0]--;
            if (smoker_count_ptr[0] == 0)
            {
                sem_post(smoker_2_sem);
                sem_post(smoker_1_sem);
            }
            sem_post(for_smoker_3_sem);
        }
    }

    exit(0);
}
