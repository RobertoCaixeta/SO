#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>
#include <limits.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct {
    int id;
    char command[256]; // Nome do executável
    int *dependencies; // Lista de dependências
    int num_dependencies; // Número de dependências
    sem_t sem; // Semáforo para controlar a execução do processo
    long exec_time; // Tempo de execução estimado
} Process;

typedef struct {
    Process *processes;
    int num_processes;
} Application;

typedef struct {
    int id;
} ProcessResult;

sem_t core_sem; // Semáforo para controlar o número de núcleos disponíveis

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

        if (strcmp(app.processes[i].command, "teste15") == 0) {
            app.processes[i].exec_time = 15;
        } else if (strcmp(app.processes[i].command, "teste30") == 0) {
            app.processes[i].exec_time = 30;
        } else {
            app.processes[i].exec_time = LONG_MAX; // valor grande para comandos desconhecidos
        }

        sem_init(&app.processes[i].sem, 0, 0);
        i++;
    }

    fclose(file);
    return app;
}

void execute_process(Process *process) {
    printf("Processo %d executando!\n", process->id);
    if(strcmp(process->command, "teste15") == 0){
        volatile long long i;
        for (i = 0; i < 8000000000; i++);
        printf("Processo %d finalizado (tempo de 15 segundos)\n", process->id);
    } else if(strcmp(process->command, "teste30") == 0){
        volatile long long i;
        for (i = 0; i < 16000000000; i++);
        printf("Processo %d finalizado (tempo de 30 segundos)\n", process->id);
    }
}

int find_next_process(Application *app, int *process_done, int *in_progress) {
    int next_process = -1;
    long min_time = LONG_MAX;

    for (int i = 0; i < app->num_processes; i++) {
        if (!process_done[i] && !in_progress[i]) {
            int can_execute = 1;
            for (int j = 0; j < app->processes[i].num_dependencies; j++) {
                int dep_id = app->processes[i].dependencies[j] - 1;
                if (dep_id >= 0 && dep_id < app->num_processes && !process_done[dep_id]) {
                    can_execute = 0;
                    break;
                }
            }
            if (can_execute && app->processes[i].exec_time < min_time) {
                min_time = app->processes[i].exec_time;
                next_process = i;
            }
        }
    }

    return next_process;
}

void run_process(Application *app, int process_index, int *process_done, int *in_progress) {
    Process *process = &app->processes[process_index];

    sem_wait(&core_sem); // Espera por um núcleo disponível

    pid_t pid = fork();
    if (pid == 0) { // Processo filho
        execute_process(process);
        sem_post(&process->sem); // Sinaliza que o processo terminou
        exit(0);
    } else if (pid > 0) { // Processo pai
        in_progress[process_index] = 1;
    } else {
        perror("Erro ao criar processo filho");
        exit(EXIT_FAILURE);
    }
}

void check_processes(Application *app, int *process_done, int *in_progress) {
    for (int i = 0; i < app->num_processes; i++) {
        if (in_progress[i]) {
            int status;
            pid_t result = waitpid(-1, &status, WNOHANG);
            if (result > 0) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    process_done[i] = 1;
                    in_progress[i] = 0;
                    sem_post(&core_sem); // Libera o núcleo
                    sem_post(&app->processes[i].sem); // Sinaliza que o processo terminou
                }
            }
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

    sem_init(&core_sem, 0, num_cores); // Inicializa o semáforo com o número de núcleos

    int *process_done = calloc(app.num_processes, sizeof(int));
    int *in_progress = calloc(app.num_processes, sizeof(int));
    if (!process_done || !in_progress) {
        perror("Erro ao alocar memória");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int next_process;
        while ((next_process = find_next_process(&app, process_done, in_progress)) != -1) {
            run_process(&app, next_process, process_done, in_progress);
        }

        check_processes(&app, process_done, in_progress);

        int all_done = 1;
        for (int i = 0; i < app.num_processes; i++) {
            if (!process_done[i]) {
                all_done = 0;
                break;
            }
        }
        if (all_done) {
            break; // Todos os processos foram executados
        }
    }

    // Libera memória alocada
    for (int i = 0; i < app.num_processes; i++) {
        free(app.processes[i].dependencies);
    }
    free(app.processes);
    free(process_done);
    free(in_progress);

    return 0;
}
