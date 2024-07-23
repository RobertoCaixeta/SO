#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>
#include <limits.h>
#include <omp.h> // Adiciona o OpenMP

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

void find_initial_processes(Application *app, int **initial_processes, int *num_initial) {
    *num_initial = 0;

    // Primeiro passe para contar o número de processos iniciais
    for (int i = 0; i < app->num_processes; i++) {
        if (app->processes[i].num_dependencies == 0) {
            (*num_initial)++;
        }
    }

    // Aloca memória suficiente de uma vez só
    *initial_processes = malloc((*num_initial) * sizeof(int));
    if (*initial_processes == NULL) {
        perror("Erro ao alocar memória");
        exit(EXIT_FAILURE);
    }

    // Segundo passe para preencher o array de processos iniciais
    int index = 0;
    for (int i = 0; i < app->num_processes; i++) {
        if (app->processes[i].num_dependencies == 0) {
            (*initial_processes)[index++] = i;
        }
    }
}

void execute_process(Process *process, int pipe_fd[2]) {
    printf("Processo %d executando!\n", process->id);
    if(strcmp(process->command, "teste15") == 0){
        volatile long long i;
        for (i=0; i<8000000000; i++);
        printf("tempo de 15 segundos\n");
    } else if(strcmp(process->command, "teste30") == 0){
        volatile long long i;
        for (i=0; i<16000000000; i++);
        printf("tempo de 30 segundos\n");
    }

    ProcessResult result;
    result.id = process->id;
    
    write(pipe_fd[1], &result, sizeof(result));
}

int find_next_process(Application *app, int *process_done) {
    int next_process = -1;
    long min_time = LONG_MAX;

    for (int i = 0; i < app->num_processes; i++) {
        if (!process_done[i]) {
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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_cores>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_cores = atoi(argv[1]);
    omp_set_num_threads(num_cores);

    const char *input_file = "entrada.txt";
    Application app = read_application_file(input_file);

    int *initial_processes;
    int num_initial;
    find_initial_processes(&app, &initial_processes, &num_initial);

    int *process_done = calloc(app.num_processes, sizeof(int));
    if (process_done == NULL) {
        perror("Erro ao alocar memória");
        exit(EXIT_FAILURE);
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        free(process_done);
        exit(EXIT_FAILURE);
    }

    struct timeval makespan_start, makespan_end;
    gettimeofday(&makespan_start, NULL);

    int processes_left = app.num_processes;
    int no_process_found = 0;

    while (processes_left > 0) {
        no_process_found = 1;
        for (int i = 0; i < num_cores; i++) {
            int next_process = find_next_process(&app, process_done);
            if (next_process != -1) {
                if (!process_done[next_process]) {
                    printf("Executando processo %d\n", app.processes[next_process].id);
                    execute_process(&app.processes[next_process], pipe_fd);
                    process_done[next_process] = 1;
                    processes_left--;
                    no_process_found = 0;
                }
            }
        }
        printf("Processos restantes: %d\n", processes_left);
        if (no_process_found) {
            printf("Nenhum processo encontrado nesta iteração.\n");
            usleep(1000); // espera 1ms
        }
    }

    gettimeofday(&makespan_end, NULL);
    long makespan = (makespan_end.tv_sec - makespan_start.tv_sec) * 1000000L + (makespan_end.tv_usec - makespan_start.tv_usec);
    printf("Makespan: %ld microseconds\n", makespan);

    // Free allocated memory
    for (int i = 0; i < app.num_processes; i++) {
        free(app.processes[i].dependencies);
    }
    free(app.processes);
    free(initial_processes);
    free(process_done);

    return 0;
}