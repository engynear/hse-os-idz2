#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/shm.h>

int sem_id;
int num_rounds;
int* smoker_count_ptr;
int smoker_1_sem = 0, smoker_2_sem = 1, for_smoker_1_sem = 3, for_smoker_2_sem = 4, smoker_3_sem = 5, for_smoker_3_sem = 6;
int customers_in_queue = 0;


void smoker_behaviour(int id);
void cleanup();
void signal_handler(int nsig);
void initialize_semaphores();
void create_shared_memory();

void mediator_behaviour(int n);

// Определение функции для освобождения всех семафоров и разделяемой памяти
void cleanup() {
    semctl(sem_id, 0, IPC_RMID, 0);
    semctl(sem_id, 1, IPC_RMID, 0);
    semctl(sem_id, 2, IPC_RMID, 0);
    semctl(sem_id, 3, IPC_RMID, 0);
    semctl(sem_id, 4, IPC_RMID, 0);
    semctl(sem_id, 5, IPC_RMID, 0);
    semctl(sem_id, 6, IPC_RMID, 0);

    shmdt(smoker_count_ptr);
}

// Обработчик сигнала SIGINT, который будет вызван при нажатии Ctrl+C
void signal_handler(int sig) {
    cleanup(); // функция для удаления всех созданных ресурсов
    exit(0); // выход из программы
}
// Функция для инициализации семафоров
void initialize_semaphores() {
    sem_id = semget(IPC_PRIVATE, 5, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Can\'t create semaphore\n");
        exit(-1);
    }

    semctl(sem_id, smoker_1_sem, SETVAL, 0);
    semctl(sem_id, smoker_2_sem, SETVAL, 0);
    semctl(sem_id, smoker_3_sem, SETVAL, 0);

    semctl(sem_id, for_smoker_1_sem, SETVAL, 0);
    semctl(sem_id, for_smoker_2_sem, SETVAL, 0);
    semctl(sem_id, for_smoker_3_sem, SETVAL, 0);
}

void wait_sem(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void signal_sem(int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}
// Функция создания разделяемой памяти и её отображения в адресное пространство текущего процесса
void create_shared_memory() {
    const int array_size = 4;
    key_t shm_key;
    int shmid;
    char pathname[]="../3-shr-sem";
    shm_key = ftok(pathname, 0);

    if((shmid = shmget(shm_key, sizeof(int)*array_size,
                       0666 | IPC_CREAT | IPC_EXCL)) < 0)  {
        if((shmid = shmget(shm_key, sizeof(int)*array_size, 0)) < 0) {
            printf("Can\'t connect to shared memory\n");
            exit(-1);
        };
        smoker_count_ptr = (int*)shmat(shmid, NULL, 0);
    } else {
        smoker_count_ptr = (int*)shmat(shmid, NULL, 0);
        for(int i = 0; i < array_size; ++i) {
            smoker_count_ptr[i] = 0;
        }
    }

    smoker_count_ptr[0] = num_rounds;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler); // регистрация обработчика сигнала SIGINT
    char *p;

    // проверка на количество аргументов командной строки
    if (argc != 2) {
        num_rounds = 5;
    } else {
        num_rounds = strtol(argv[1], &p, 10);
    }

    cleanup(); // очистка ресурсов, если они были созданы ранее
    initialize_semaphores(); // создание семафоров
    create_shared_memory(); // создание общей памяти

    pid_t smoker_1 = fork(); // создание процесса для первого курильщика

    if (smoker_1 < 0) {
        perror("Can\'t fork smoker_1 1");
        exit(-1);
    } else if (smoker_1 == 0) { // процесс первого курильщика
        smoker_behaviour(1); // поведение первого курильщика
        exit(0); // выход из программы
    } else {
        pid_t smoker_2 = fork(); // создание процесса для второго курльщика

        if (smoker_2 == -1) {
            perror("Can\'t fork smoker_1 2");
            exit(-1);
        } else if (smoker_2 == 0) { // процесс второго курильщика
            smoker_behaviour(2); // поведение второго курильщика
            exit(0); // выход из программы
        } else { // процесс посетителей
            pid_t smoker_3 = fork(); // создание процесса для третьего курильщика
            if (smoker_3 == -1) {
                perror("Can\'t fork smoker_1 2");
                exit(-1);
            } else if (smoker_3 == 0) { // процесс третьего курильщика
                smoker_behaviour(3); // поведение третьего курильщика
                exit(0); // выход из программы
            } else {
                // создание процесса посредника
                pid_t mediator = fork();
                if (mediator == -1) {
                    perror("Can\'t fork mediator");
                    exit(-1);
                } else if (mediator == 0) {
                    mediator_behaviour(num_rounds); // поведение посредника
                    exit(0); // выход из программы
                }

                for (int i = 1; i <= 4; i++) {
                    wait(0); // ожидание завершения всех процессов курильщиков и посредника
                }

                cleanup(); // удаление всех созданных ресурсов
                printf("Все раунды завершены.\n");
                exit(0); // выход из программы
            }
        }
    }

}

void mediator_behaviour(int n) {
    srand(time(NULL));
    while (n > 0) {
        int random_number = rand() % 3 + 1;
        if (random_number == 1) {
            fflush(stdout);
            printf("Посредник положил на стол бумагу и спички\n");
            signal_sem(smoker_1_sem);
            wait_sem(for_smoker_1_sem);
            fflush(stdout);
        } else if (random_number == 2) {
            fflush(stdout);
            printf("Посредник положил на стол табак и спички\n");
            signal_sem(smoker_2_sem);
            wait_sem(for_smoker_2_sem);
            fflush(stdout);
        } else {
            fflush(stdout);
            printf("Посредник положил на стол табак и бумагу\n");
            signal_sem(smoker_3_sem);
            wait_sem(for_smoker_3_sem);
            fflush(stdout);
        }
        sleep(1);
        printf("Раунд пройден.\n");
        n--;
    }
    exit(0);
}


void smoker_behaviour(int id) {
    fflush(stdout);
    printf("Курильщик номер %d готов, у него есть табак.\n", id);
    int *pInt = smoker_count_ptr;
    while (pInt[0] > 0) {
        if (id == 1) {
            sleep(1);
            if (pInt[0] == 0) {
                exit(0);  // Если значение <= 0, завершаем цикл
            }

            wait_sem(smoker_1_sem);
            if (pInt[0] == 0) {
                exit(0);
            }
            fflush(stdout);
            printf("Курильщик номер %d забирает со стола бумагу и спички и скуривает сигарету.\n", id);
            sleep(1);
            pInt[0]--;
            if (pInt[0] == 0) {
                signal_sem(smoker_2_sem);
                signal_sem(smoker_3_sem);
            }
            signal_sem(for_smoker_1_sem);
        } else if (id == 2) {
            sleep(1);
            if (pInt[0] == 0) {
                exit(0);  // Если значение <= 0, завершаем цикл
            }

            wait_sem(smoker_2_sem);
            fflush(stdout);
            printf("Курильщик номер %d забирает со стола табак и спички и скуривает сигарету.\n", id);
            sleep(1);
            pInt[0]--;
            if (pInt[0] == 0) {
                signal_sem(smoker_1_sem);
                signal_sem(smoker_3_sem);
            }
            signal_sem(for_smoker_2_sem);
        } else {
            sleep(1);
            if (pInt[0] == 0) {
                exit(0);  // Если значение <= 0, завершаем цикл
            }

            wait_sem(smoker_3_sem);
            fflush(stdout);
            printf("Курильщик номер %d забирает со стола табак и бумагу и скуривает сигарету.\n", id);
            sleep(1);
            pInt[0]--;
            if (pInt[0] == 0) {
                signal_sem(smoker_2_sem);
                signal_sem(smoker_1_sem);
            }
            signal_sem(for_smoker_3_sem);
        }
    }

    exit(0);
}
