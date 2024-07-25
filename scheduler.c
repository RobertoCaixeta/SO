/*Integrantes do grupo
Henrique de Miranda Carrer - 180101951
Theo Marques da Rocha - 190038489 
Lucas Aquino Costa - 190055146
Roberto Caixeta Ribeiro Oliveira - 190019611

gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2)
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED,
    WAITING
} ProcessStatus;


typedef struct {
    int id;
    char command[256]; // Nome do executável
    int *dependencies; // Lista de dependências
    int num_dependencies; // Número de dependências
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
    //Abre o arquivo e printa mensagem em caso de erro
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    Application app;
    app.num_processes = 0;

    //Define o número de processos pela quantidade de linhas do arquivo
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        app.num_processes++;
    }

    //Volta o ponteiro para o início do arquivo
    rewind(file);

    //Aloca a memória necessária para alocar os processos na estrutura desejada
    app.processes = malloc(sizeof(Process) * app.num_processes);
    if (app.processes == NULL) {
        perror("Erro ao alocar memória");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    //Loop para ler o arquivo e armazenar suas informações nas estruturas desejadas
    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        //Pega o id do processo
        char *token = strtok(line, " ");
        app.processes[i].id = atoi(token);

        // Pega o comando do processo, se é "teste15" ou "teste30"
        token = strtok(NULL, " ");
        strcpy(app.processes[i].command, token);

        token = strtok(NULL, " ");
        app.processes[i].num_dependencies = 0;

        //Aloca a memória para as dependências do processo
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

        //Loop para anotar as dependências do processo até encontrar o caracter '#'
        char *dep_token = strtok(token, ",");
        while (dep_token != NULL && strcmp(dep_token, "#") != 0) {
            app.processes[i].dependencies[app.processes[i].num_dependencies++] = atoi(dep_token);
            dep_token = strtok(NULL, ",");
        }

        //Inicia o processo como WAITING
        app.processes[i].status = WAITING;
        i++;
    }

    //Fecha o arquivo e retorna os dados do arquivo já armazenados
    fclose(file);
    return app;
}


//Função para simular a execução de um processo
void execute_process(Process *process, int pipe_fd[2]) {
    //Muda o status do processo para RUNNING
    process->status = RUNNING;
    printf("Processo %d executando...\n", process->id);

    //Confere qual é o comando do processo e executa o Loop apropriado
    if(strcmp(process->command, "teste15") == 0){
        for(long long i = 0 ; i < 8000000000 ; i++);
    }

    else if(strcmp(process->command, "teste30") == 0){
        for(long long i = 0 ; i < 16000000000; i++);
    }

    
    //Após a execução do processo se escreve no pipe que ele foi executado passando o id do processo
    ProcessResult result;
    result.id = process->id;
    write(pipe_fd[1], &result, sizeof(result));

    //Muda o status do processo apra FINISHED
    process->status = FINISHED;
}


//Escalonador da aplicação
void schedule_application(Application *app, int num_cores) {
    //Cria variável para controle se os processos estão todos prontos
    int all_done = 0;
    int pipe_fd[2];

    //Cria pipe para comunicação de fim de processo
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    //Inicia contador de controle de tempo
    struct timeval makespan_start, makespan_end;
    gettimeofday(&makespan_start, NULL);

    //Variável para o controle de quantos cores estão sendo usados no momento
    int cores_used = 0;
    while (!all_done) {
        all_done = 1;
        //Loop para passar pelos processos e mandar executar os que estão READY
        for (int i = 0; i < app->num_processes; i++) {
            //Condição para saber se o processo não foi executado ainda e se há cores disponíveis
            if (!(app->processes[i].status == FINISHED) && cores_used < num_cores) {
                app->processes[i].status = READY;
                // Loop para verificar se todas dependências do processo já foram executadas, caso sim, 
                // o processo é movido para READY, caso não continua em blocked
                for (int j = 0; j < app->processes[i].num_dependencies; j++) {
                    if (!(app->processes[app->processes[i].dependencies[j] - 1].status == FINISHED) && app->processes[i].dependencies[j] != 0) {
                        app->processes[i].status = BLOCKED;
                        break;
                    }
                }

                //Caso o processo esteja no status READY ele é executado
                if (app->processes[i].status == READY) {
                    {
                        pid_t pid = fork();
                        // Processo filho
                        if (pid == 0) { 
                            // Fechar o lado de leitura do pipe no processo filho
                            close(pipe_fd[0]);
                            //Chama a função que simula execução do processo
                            execute_process(&app->processes[i], pipe_fd);
                            // Fechar o lado de escrita do pipe no processo filho
                            close(pipe_fd[1]);
                            exit(0);
                        } 
                        //Processo pai
                        else if (pid > 0) {
                            cores_used++;
                        } else {
                            perror("fork");
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
            //Leitura do pipe para comunicação do fim da execução de um processo
            if (read(pipe_fd[0], &result, sizeof(result)) > 0) {
                {
                    //Como o processo foi finalizado pode se liberar um core
                    cores_used--;

                    // Marque o processo como concluído e muda seu status para FINISHED
                    for (int i = 0; i < app->num_processes; i++) {
                        if (app->processes[i].id == result.id) {
                            app->processes[i].status = FINISHED;
                            printf("Processo %d executado!\n", result.id);
                            break;
                        }
                    }

                    // Verifica se todos os processos terminaram
                    all_done = 1;
                    for (int i = 0; i < app->num_processes; i++) {
                        if (app->processes[i].status != FINISHED) {
                            all_done = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    //Leitura do tempo total de execução dos processos
    gettimeofday(&makespan_end, NULL);
    double makespan = (makespan_end.tv_sec - makespan_start.tv_sec) + (makespan_end.tv_usec - makespan_start.tv_usec) / 1000000.0;

    printf("Makespan: %.2f segundos\n", makespan);

    close(pipe_fd[0]);
    close(pipe_fd[1]);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <num_cores> <arquivo_entrada>\n", argv[0]);
        return 1;
    }

    //Leitura dos parâmetros passados pela linha de comando
    int num_cores = atoi(argv[1]);
    const char *filename = argv[2];

    //Chamada da função de leitura do arquivo
    Application app = read_application_file(filename);

    //Execução do escalonador
    schedule_application(&app, num_cores);

    //Liberar a memória alocada
    for (int i = 0; i < app.num_processes; i++) {
        free(app.processes[i].dependencies);
    }
    free(app.processes);

    return 0;
}
