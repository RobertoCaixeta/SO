#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>


typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessStatus;


typedef struct {
    int id;
    char command[256]; // Nome do executável
    int *dependencies; // Lista de dependências
    int num_dependencies; // Número de dependências
    sem_t sem; // Semáforo para controlar a execução do processo
    ProcessStatus status; // Processo status
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
        app.processes[i].status = BLOCKED;
        i++;
    }

    fclose(file);
    return app;
}

void execute_process(Process *process, int pipe_fd[2]) {
    process->status = RUNNING;
    printf("Processo %d executando...\n", process->id);
    if(strcmp(process->command, "teste15") == 0){
        //for(long long i = 0 ; i < 8000000000 ; i++);
        sleep(15);
    }

    else if(strcmp(process->command, "teste30") == 0){
        //for(long long i = 0 ; i < 16000000000; i++);
        sleep(30);
    }

    
    
    ProcessResult result;
    result.id = process->id;
    
    write(pipe_fd[1], &result, sizeof(result));
    process->status = FINISHED;
}

void schedule_application(Application *app, int num_cores) {
    int *process_done = calloc(app->num_processes, sizeof(int));
    if (process_done == NULL) {
        perror("Erro ao alocar memória");
        exit(EXIT_FAILURE);
    }
    
    int all_done = 0;
    int pipe_fd[2];

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        free(process_done);
        exit(EXIT_FAILURE);
    }

    struct timeval makespan_start, makespan_end;
    gettimeofday(&makespan_start, NULL);

    int cores_used = 0;
    while (!all_done) {
        all_done = 1;
        for (int i = 0; i < app->num_processes; i++) {
            if (!process_done[i] && cores_used < num_cores) {
                app->processes[i].status = READY;
                // Loop para verificar se todas dependências do processo já foram executadas
                for (int j = 0; j < app->processes[i].num_dependencies; j++) {
                    if (!process_done[app->processes[i].dependencies[j] - 1] && app->processes[i].dependencies[j] != 0) {
                        app->processes[i].status = BLOCKED;
                        break;
                    }
                }

                if (app->processes[i].status == READY) {
                    {
                        pid_t pid = fork();

                        if (pid == 0) { // Child process
                            close(pipe_fd[0]); // Fechar o lado de leitura do pipe no processo filho
                            sem_post(&app->processes[i].sem);
                            execute_process(&app->processes[i], pipe_fd);
                            close(pipe_fd[1]); // Fechar o lado de escrita do pipe no processo filho
                            exit(0);
                        } else if (pid > 0) { // Parent process
                            cores_used++;
                        } else {
                            perror("fork");
                            free(process_done);
                            close(pipe_fd[0]);
                            close(pipe_fd[1]);
                            exit(EXIT_FAILURE);
                        }
                    }
                } else {
                    all_done = 0;
                }
            }
        }

        // Aguarda processos filhos terminarem e lê tempos de execução
        while (cores_used > 0) {
            ProcessResult result;
            if (read(pipe_fd[0], &result, sizeof(result)) > 0) {
                {
                    cores_used--;

                    // Marque o processo como concluído
                    for (int i = 0; i < app->num_processes; i++) {
                        if (app->processes[i].id == result.id) {
                            process_done[i] = 1;
                            printf("Processo %d executado!\n", result.id);
                            break;
                        }
                    }

                    // Verifica se todos os processos terminaram
                    all_done = 1;
                    for (int i = 0; i < app->num_processes; i++) {
                        if (!process_done[i]) {
                            all_done = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    gettimeofday(&makespan_end, NULL);
    double makespan = (makespan_end.tv_sec - makespan_start.tv_sec) + (makespan_end.tv_usec - makespan_start.tv_usec) / 1000000.0;

    printf("Makespan: %.2f segundos\n", makespan);

    free(process_done);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <num_cores> <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    int num_cores = atoi(argv[1]);
    const char *filename = argv[2];

    Application app = read_application_file(filename);

    schedule_application(&app, num_cores);

    for (int i = 0; i < app.num_processes; i++) {
        sem_destroy(&app.processes[i].sem);
        free(app.processes[i].dependencies);
    }
    free(app.processes);

    return 0;
}
