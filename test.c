#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>
#include <limits.h>
#include <sys/wait.h>
#include <signal.h>

#define WAITING 'W'
#define READY 'R'
#define EXECUTING 'E'
#define FINISHED 'F'
#define MAX_DEPENDENCY_LEVELS 100 // Ajuste conforme necessário

typedef struct {
    int id;
    char command[256]; // Nome do executável
    int *dependencies; // Lista de dependências
    int num_dependencies; // Número de dependências
    sem_t sem; // Semáforo para controlar a execução do processo
    long exec_time; // Tempo de execução estimado
    char status;
    pid_t pid; // PID do processo
    long actual_exec_time; // Tempo de execução real
} Process;

typedef struct {
    Process *processes;
    int num_processes;
} Application;

typedef struct ReadyProcessNode {
    int process_index;
    struct ReadyProcessNode *next;
} ReadyProcessNode;

typedef struct DependencyQueue {
    ReadyProcessNode *head;
    ReadyProcessNode *tail;
} DependencyQueue;

void initialize_queue(DependencyQueue *queue) {
    queue->head = NULL;
    queue->tail = NULL;
}

void enqueue_process(DependencyQueue *queue, int process_index) {
    ReadyProcessNode *node = (ReadyProcessNode *)malloc(sizeof(ReadyProcessNode));
    node->process_index = process_index;
    node->next = NULL;
    if (queue->tail == NULL) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
}

int dequeue_process(DependencyQueue *queue) {
    if (queue->head == NULL) {
        return -1;
    }
    ReadyProcessNode *node = queue->head;
    int process_index = node->process_index;
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    free(node);
    return process_index;
}

Application read_application_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    Application app;
    app.num_processes = 0;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        app.num_processes++;
    }

    rewind(file);

    app.processes = malloc(sizeof(Process) * app.num_processes);
    if (app.processes == NULL) {
        perror("Erro ao alocar memória");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, " ");
        app.processes[i].id = atoi(token);

        token = strtok(NULL, " ");
        strcpy(app.processes[i].command, token);

        token = strtok(NULL, " ");
        app.processes[i].num_dependencies = 0;
        app.processes[i].dependencies = malloc(sizeof(int) * app.num_processes);
        if (app.processes[i].dependencies == NULL) {
            perror("Erro ao alocar memória");
            fclose(file);
            for (int j = 0; j <= i; j++) {
                free(app.processes[j].dependencies);
            }
            free(app.processes);
            exit(EXIT_FAILURE);
        }

        char *dep_token = strtok(token, ",");
        while (dep_token != NULL && strcmp(dep_token, "#") != 0) {
            app.processes[i].dependencies[app.processes[i].num_dependencies++] = atoi(dep_token);
            dep_token = strtok(NULL, ",");
        }

        if (app.processes[i].num_dependencies == 0) {
            app.processes[i].status = READY;
        } else {
            app.processes[i].status = WAITING;
        }

        if (strcmp(app.processes[i].command, "teste15") == 0) {
            app.processes[i].exec_time = 15;
        } else if (strcmp(app.processes[i].command, "teste30") == 0) {
            app.processes[i].exec_time = 30;
        } else {
            app.processes[i].exec_time = LONG_MAX; // valor grande para comandos desconhecidos
        }

        sem_init(&app.processes[i].sem, 0, 0);
        app.processes[i].pid = -1; // Inicializa o PID como -1 (não iniciado)
        app.processes[i].actual_exec_time = 0; // Inicializa o tempo de execução real como 0
        i++;
    }

    fclose(file);
    return app;
}

void execute_process(Process *process) {
    printf("Processo %d executando!\n", process->id);
    if (strcmp(process->command, "teste15") == 0) {
        long long i;
        for (i = 0; i < 8000000000; i++);
        printf("Processo %d finalizado (tempo de 15 segundos)\n", process->id);
    } else if (strcmp(process->command, "teste30") == 0) {
        long long i;
        for (i = 0; i < 16000000000; i++);
        printf("Processo %d finalizado (tempo de 30 segundos)\n", process->id);
    }
}

void print_application(const Application *app) {
    for (int i = 0; i < app->num_processes; i++) {
        Process p = app->processes[i];
        printf("Process ID: %d\n", p.id);
        printf("Command: %s\n", p.command);
        printf("Dependencies: ");
        for (int j = 0; j < p.num_dependencies; j++) {
            printf("%d ", p.dependencies[j]);
        }
        printf("\n");
        printf("Number of Dependencies: %d\n", p.num_dependencies);
        printf("Execution Time: %ld\n", p.exec_time);
        printf("Actual Execution Time: %ld microseconds\n", p.actual_exec_time);
        printf("Status: %c\n", p.status);
        printf("PID: %d\n\n", p.pid);
    }
}

void update_process_status(Process *process, char new_status) {
    process->status = new_status;
}

void turn_first_processes_ready(Application *app, DependencyQueue *ready_queues) {
    int num_processes = app->num_processes;

    for (int i = 0; i < num_processes; i++) {
        if (app->processes[i].num_dependencies == 0) {
            update_process_status(&app->processes[i], READY);
            enqueue_process(&ready_queues[0], i);
        }
    }
}

int are_dependencies_finished(Process *process, Application *app) {
    for (int i = 0; i < process->num_dependencies; i++) {
        int dep_id = process->dependencies[i];
        for (int j = 0; j < app->num_processes; j++) {
            if (app->processes[j].id == dep_id && app->processes[j].status != FINISHED) {
                return 0; // Dependência não finalizada
            }
        }
    }
    return 1; // Todas as dependências finalizadas
}

void find_ready_processes(Application *app, DependencyQueue *ready_queues) {
    int num_processes = app->num_processes;

    for (int i = 0; i < num_processes; i++) {
        Process *process = &app->processes[i];
        if (process->status == WAITING && are_dependencies_finished(process, app)) {
            update_process_status(process, READY);
            int dependency_level = 0;
            for (int j = 0; j < process->num_dependencies; j++) {
                int dep_id = process->dependencies[j];
                for (int k = 0; k < num_processes; k++) {
                    if (app->processes[k].id == dep_id) {
                        dependency_level = (dependency_level > app->processes[k].num_dependencies) ? dependency_level : app->processes[k].num_dependencies;
                    }
                }
            }
            enqueue_process(&ready_queues[dependency_level + 1], i);
        }
    }
}

void sort_ready_queue_by_exec_time(DependencyQueue *queue, Process *processes) {
    if (queue->head == NULL || queue->head->next == NULL) {
        return;
    }

    ReadyProcessNode *sorted = NULL;

    while (queue->head != NULL) {
        ReadyProcessNode *node = queue->head;
        queue->head = queue->head->next;

        if (sorted == NULL || processes[node->process_index].exec_time < processes[sorted->process_index].exec_time) {
            node->next = sorted;
            sorted = node;
        } else {
            ReadyProcessNode *current = sorted;
            while (current->next != NULL && processes[current->next->process_index].exec_time < processes[node->process_index].exec_time) {
                current = current->next;
            }
            node->next = current->next;
            current->next = node;
        }
    }

    queue->head = sorted;
    queue->tail = sorted;
    while (queue->tail->next != NULL) {
        queue->tail = queue->tail->next;
    }
}

void execute_parallel(Application *app, int num_cores) {
    int active_processes = 0;
    int processes_remaining = app->num_processes;

    DependencyQueue ready_queues[MAX_DEPENDENCY_LEVELS];
    for (int i = 0; i < MAX_DEPENDENCY_LEVELS; i++) {
        initialize_queue(&ready_queues[i]);
    }

    turn_first_processes_ready(app, ready_queues);

    while (processes_remaining > 0) {
        find_ready_processes(app, ready_queues);

        for (int i = 0; i < MAX_DEPENDENCY_LEVELS; i++) {
            sort_ready_queue_by_exec_time(&ready_queues[i], app->processes);
        }

        for (int i = 0; i < MAX_DEPENDENCY_LEVELS && active_processes < num_cores; i++) {
            while (active_processes < num_cores && ready_queues[i].head != NULL) {
                int next_process = dequeue_process(&ready_queues[i]);

                struct timeval start, end;
                gettimeofday(&start, NULL);

                pid_t pid = fork();
                if (pid == 0) {
                    // Processo filho executa a tarefa
                    execute_process(&app->processes[next_process]);
                    gettimeofday(&end, NULL);
                    long exec_time = (end.tv_sec - start.tv_sec);
                    exit(exec_time);
                } else if (pid > 0) {
                    // Processo pai continua
                    app->processes[next_process].pid = pid; // Armazena o PID do processo filho
                    active_processes++;
                    processes_remaining--;
                } else {
                    perror("Erro ao criar o processo");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Espera por qualquer processo filho terminar
        if (active_processes > 0) {
            int status;
            pid_t pid = wait(&status);
            if (pid > 0) {
                // Marca o processo terminado como FINISHED
                for (int i = 0; i < app->num_processes; i++) {
                    if (app->processes[i].pid == pid) {
                        if (WIFEXITED(status)) {
                            app->processes[i].actual_exec_time = WEXITSTATUS(status);
                        }
                        update_process_status(&app->processes[i], FINISHED);
                        app->processes[i].pid = -1; // Reseta o PID
                        break;
                    }
                }
                active_processes--;
            }
        }
    }

    // Espera todos os processos filhos terminarem
    while (active_processes > 0) {
        int status;
        pid_t pid = wait(&status);
        if (pid > 0) {
            // Marca o processo terminado como FINISHED
            for (int i = 0; i < app->num_processes; i++) {
                if (app->processes[i].pid == pid) {
                    if (WIFEXITED(status)) {
                        app->processes[i].actual_exec_time = WEXITSTATUS(status);
                    }
                    update_process_status(&app->processes[i], FINISHED);
                    app->processes[i].pid = -1; // Reseta o PID
                    break;
                }
            }
            active_processes--;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_cores>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int num_cores = atoi(argv[1]);
    const char *input_file = "entrada.txt";
    Application app = read_application_file(input_file);

    execute_parallel(&app, num_cores);
    print_application(&app);

    // Libere memória alocada
    for (int i = 0; i < app.num_processes; i++) {
        free(app.processes[i].dependencies);
    }
    free(app.processes);

    return 0;
}
