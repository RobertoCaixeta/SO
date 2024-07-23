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
        for(long long i = 0 ; i < 8000000000 ; i++);
    } else if(strcmp(process->command, "teste30") == 0){
        for(long long i = 0 ; i < 16000000000; i++);
    }

    ProcessResult result;
    result.id = process->id;
    
    write(pipe_fd[1], &result, sizeof(result));
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

    #pragma omp parallel
    {
        while (num_initial > 0) {
            #pragma omp single
            {
                // Find the process with the shortest job
                int shortest_job_index = -1;
                long long shortest_job_duration = LLONG_MAX;

                for (int i = 0; i < num_initial; i++) {
                    int process_index = initial_processes[i];
                    long long job_duration = (strcmp(app.processes[process_index].command, "teste15") == 0) ? 8000000000LL : 16000000000LL;

                    if (job_duration < shortest_job_duration) {
                        shortest_job_duration = job_duration;
                        shortest_job_index = i;
                    }
                }

                int process_index = initial_processes[shortest_job_index];
                #pragma omp task firstprivate(process_index)
                {
                    execute_process(&app.processes[process_index], pipe_fd);
                    process_done[process_index] = 1;

                    // Remove the executed process from the initial_processes array
                    for (int i = shortest_job_index; i < num_initial - 1; i++) {
                        initial_processes[i] = initial_processes[i + 1];
                    }
                    num_initial--;

                    // Check for new initial processes whose dependencies are now met
                    for (int i = 0; i < app.num_processes; i++) {
                        if (!process_done[i]) {
                            int dependencies_met = 1;
                            for (int j = 0; j < app.processes[i].num_dependencies; j++) {
                                if (!process_done[app.processes[i].dependencies[j]]) {
                                    dependencies_met = 0;
                                    break;
                                }
                            }
                            if (dependencies_met) {
                                num_initial++;
                                initial_processes = realloc(initial_processes, num_initial * sizeof(int));
                                initial_processes[num_initial - 1] = i;
                            }
                        }
                    }
                }
            }
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