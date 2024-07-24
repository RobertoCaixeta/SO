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
    char status; // R = Ready / W = Waiting / E = Executing / F = Finished
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
    // setta numero de processos como o numero de linhas do arquivo
    while (fgets(line, sizeof(line), file)) {
        app.num_processes++;
    }

    rewind(file);

    // aloca espaco para os processos na memoria
    app.processes = malloc(sizeof(Process) * app.num_processes);
    if (app.processes == NULL) {
        perror("Erro ao alocar memória");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, " ");
        // setta o id do processo
        app.processes[i].id = atoi(token);

        token = strtok(NULL, " ");
        // setta o programa a ser executado pelo processo
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
        while (dep_token != NULL) {
            printf("dep_token: %s\n", dep_token);
            if(atoi(dep_token) != atoi("#")) {
                // printf("strcmp(0, #): %d", strcmp("0", "#"));
                app.processes[i].dependencies[app.processes[i].num_dependencies++] = atoi(dep_token);
                if(app.processes[i].dependencies[0] == 0) {
                    // printf("Processo %d pronto para ser executado\n", app.processes[i].id);
                    app.processes[i].status = 'R';
                } else {
                    // printf("Processo %d em wait\n", app.processes[i].id);
                    app.processes[i].status = 'W';
                }
                // if(strcmp(dep_token, "#") == 0) break;
                dep_token = strtok(NULL, ",");
            } else break;
        }
        int j = 0;
        while(j < app.processes[i].num_dependencies) {
            printf("dependencias do processo %d = %d\n", app.processes[i].id, app.processes[i].dependencies[j]);
            j++;
        }

        // char *dep_token = strtok(token, ","); // corta as dependencias em A, B, C
        //     printf("token: %s\n", token);
        //     printf("dep_token: %s\n", dep_token);
        // // printf("strcmp(#, #): %d", strcmp("#\n", "#"));
        // int j=0;
        // while (dep_token != NULL && strcmp(dep_token, "#") != 0) {
        //     app.processes[i].dependencies[j] = atoi(dep_token);
        //     printf("app.processes[%d].dependencies[%d]: %d\n", i, j, app.processes[i].dependencies[j]);
        //     j++;
        //     // printf("dependencias do processo %d = %d\n", app.processes[i].id, *app.processes[i].dependencies);
        //     // if(app.processes[i].dependencies[0] == 0) {
        //     //     // printf("Processo %d pronto para ser executado\n", app.processes[i].id);
        //     //     app.processes[i].status = 'R';
        //     // } else {
        //     //     // printf("Processo %d em wait\n", app.processes[i].id);
        //     //     app.processes[i].status = 'W';
        //     // }
        //     dep_token = strtok(NULL, ",");
        // }
        printf("-------------------------------\n");
        // int j=0;
        // while(dep_token[j] != NULL) {
        //     printf("%c\n", dep_token[j]);
        //         printf("strcmp(dep_token[j], ','): %d\n", strcmp(dep_token[j], ','));
        //     // if(strcmp(dep_token[j], ",") != 0) {
        //     //     app.processes[i].num_dependencies = 1;
        //     //     // app.processes[i].dependencies
        //     // }
        //     j++;
        // }
        // printf("%d", app.processes[i].num_dependencies);
        // app.processes[i].dependencies = malloc(sizeof(int) * app.processes[i].num_dependencies);

        // j=0;
        // int dep_index = 0;
        // while(dep_token[j] != NULL) {
        //     printf("%c\n", dep_token[j]);
        //     if(strcmp(dep_token[j], ",") != 0) {
        //         app.processes[i].dependencies[dep_index] = dep_token[j];
        //         printf("%d\n", app.processes[i].dependencies[dep_index]);
        //         dep_index++;
        //     }
        //     j++;
        // }
        










        // if(strcmp(dep_token, "#") == 0) continue;
        // else {
            
        // }
        // while(dep_token != NULL && strcmp(dep_token, "#") != 0) {
        //     printf("%s", dep_token);
        // }


        // app.processes[i].num_dependencies = 0;
        // app.processes[i].dependencies = malloc(sizeof(int) * app.num_processes);
        // if (app.processes[i].dependencies == NULL) {
        //     perror("Erro ao alocar memória");
        //     fclose(file);
        //     for (int j = 0; j <= i; j++) {
        //         free(app.processes[j].dependencies);
        //     }
        //     free(app.processes);
        //     exit(EXIT_FAILURE);
        // }


    i++;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_cores>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_cores = atoi(argv[1]);
    omp_set_num_threads(num_cores);

    const char *input_file = "entrada.txt";

    // le arquivo e setta os processos independentes como R e o resto como W
    Application app = read_application_file(input_file);
}