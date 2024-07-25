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

typedef struct {
    int id;
    char command[256]; // Nome do executável
    int *dependencies; // Lista de dependências
    int num_dependencies; // Número de dependências
    sem_t sem; // Semáforo para controlar a execução do processo
    long exec_time; // Tempo de execução estimado
    char status;
    pid_t pid; // PID do processo
} Process;

typedef struct {
    Process *processes;
    int num_processes;
} Application;

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
        printf("Status: %c\n", p.status);
        printf("PID: %d\n\n", p.pid);
    }
}

void update_process_status(Process *process, char new_status) {
    process->status = new_status;
}

void turn_first_processes_ready(Application *app) {
    int num_processes = app->num_processes;

    for (int i = 0; i < num_processes; i++) {
        if (app->processes[i].num_dependencies == 0) {
            update_process_status(&app->processes[i], READY);
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

int find_next_process(Application *app) {
    int next_process = -1;
    long min_time = LONG_MAX;
    int num_processes = app->num_processes;

    for (int i = 0; i < num_processes; i++) {
        Process *process = &app->processes[i];
        if (process->status == WAITING && are_dependencies_finished(process, app)) {
            update_process_status(process, READY);
        }
        if (process->status == READY && process->exec_time < min_time) {
            min_time = process->exec_time;
            next_process = i;
        }
    }

    if (next_process != -1) {
        update_process_status(&app->processes[next_process], EXECUTING);
    }

    return next_process;
}

void execute_parallel(Application *app, int num_cores) {
    int active_processes = 0;
    int processes_remaining = app->num_processes;

    while (processes_remaining > 0) {
        while (active_processes < num_cores && processes_remaining > 0) {
            int next_process = find_next_process(app);
            if (next_process == -1) {
                break; // Não há mais processos prontos para executar
            }

            pid_t pid = fork();
            if (pid == 0) {
                // Processo filho executa a tarefa
                execute_process(&app->processes[next_process]);
                exit(0);
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

        // Espera por qualquer processo filho terminar
        if (active_processes > 0) {
            int status;
            pid_t pid = wait(&status);
            if (pid > 0) {
                // Marca o processo terminado como FINISHED
                for (int i = 0; i < app->num_processes; i++) {
                    if (app->processes[i].pid == pid) {
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
    turn_first_processes_ready(&app);

    execute_parallel(&app, num_cores);

    print_application(&app);

    // Libere memória alocada
    for (int i = 0; i < app.num_processes; i++) {
        free(app.processes[i].dependencies);
    }
    free(app.processes);

    return 0;
}
