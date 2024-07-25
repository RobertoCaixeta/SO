// ==================================================================== //
// - Henrique de Miranda Carrer - 180101951                             //
// - Lucas Aquino Costa - 190055146                                     //
// - Roberto Caixeta Ribeiro Oliveira - 190019611                       //
// - Theo Marques da Rocha - 190038489                                  //
// ==================================================================== //
// - Compilador: gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.1)      //
// - Sistema Operacional: Ubuntu 20.04.3 LTS                            //
// ==================================================================== //
// - Estratégia de escalonamento escolhida: Shortest Job First          //
// ==================================================================== //
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>
#include <limits.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

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
    float real_exec_time; // Tempo de execução real
    char status;
    pid_t pid; // PID do processo
} Process;

typedef struct {
    Process *processes;
    int num_processes;
    int *execution_order;
    int total_executed;
} Application;

struct timeval makespan_start, makespan_end;

float time_diff(struct timeval *start, struct timeval *end) {
  return (end->tv_sec - start->tv_sec) + 1e-6 * (end->tv_usec - start->tv_usec);
}

int isNumber(char *str) {
    int length = strlen(str);
    if(length == 0) return 0;

    if(str[0] == '-') {
        if(length == 1 || str[1] == '0') return 0;

        for(int i=1; i<length; i++) {
            if(!isdigit(str[i])) return 0;
        }
    } else {
        if(str[0] == '0' && length > 1) return 0;
        
        for(int i=0; i<length; i++) {
            if(!isdigit(str[i])) return 0;
        }
    }

    return 1;
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

    app.execution_order = malloc(sizeof(int) * app.num_processes);
    app.total_executed = 0;

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
            if(!isNumber(dep_token)) break;
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

void showExecutionOrder(Application *app) {
    printf("Process execution order = ");
    for(int i=0; i<app->num_processes; i++) {
        printf("%d ", app->execution_order[i]);
        if(i < app->num_processes - 1) printf("-> ");
    }
    printf("\n");
}

void execute_process(Process *process) {
    printf("Processo %d executando\n", process->id);
    if (strcmp(process->command, "teste15") == 0) {

        long long i;
        for (i = 0; i < 5000000000; i++);
        printf("Processo %d finalizado (tempo de 15 segundos)\n", process->id);
    } else if (strcmp(process->command, "teste30") == 0) {
        long long i;
        for (i = 0; i < 10000000000; i++);
        printf("Processo %d finalizado (tempo de 30 segundos)\n", process->id);
        
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

void sets_process_as_finished(Application *app, pid_t pid) {
    for (int i = 0; i < app->num_processes; i++) {
        if (app->processes[i].pid == pid) {
            update_process_status(&app->processes[i], FINISHED);
            app->execution_order[app->total_executed++] = app->processes[i].id;
            app->processes[i].pid = -1; // Reseta o PID
            break;
        }
    }
}

void save_real_exec_time(Application *app, int index, float time_diff) {
    app->processes[index].real_exec_time = time_diff;
    // printf("aqui aquino %f\n", app->processes[index].real_exec_time);
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
                // if(processes_remaining == app->num_processes) {
                //     // TODO: init makespan
                //     printf("Comecando a medir o makespan aqui\n");
                // }
                // Processo filho executa a tarefa
                struct timeval start, end;
                gettimeofday(&start, NULL);
                execute_process(&app->processes[next_process]);
                gettimeofday(&end, NULL);
                printf("porra %f\n", time_diff(&start, &end));
                // app->processes[next_process].real_exec_time = time_diff(&start, &end);
                save_real_exec_time(app, next_process, time_diff(&start, &end));
                printf("aqui aquino %f\n", app->processes[next_process].real_exec_time);
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
                sets_process_as_finished(app, pid);
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
                sets_process_as_finished(app, pid);
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

    struct timeval start;
    struct timeval end;

    gettimeofday(&start, NULL);
    execute_parallel(&app, num_cores);
    gettimeofday(&end, NULL);

    // print_application(&app);
    // showExecutionOrder(&app);

    float makespan = time_diff(&start, &end);
    printf("Makespan total: %0.2f segundos\n", makespan);
    for (int i = 0; i < app.num_processes; i++) {
        printf("Tempo de execucao do processo %d: %0.2f segundos\n", app.processes[i].id, app.processes[i].real_exec_time);
    }

    // Libere memória alocada
    for (int i = 0; i < app.num_processes; i++) {
        free(app.processes[i].dependencies);
    }
    free(app.processes);
    free(app.execution_order);

    return 0;
}
