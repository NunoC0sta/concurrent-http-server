# Servidor Web Multi-Threaded com IPC e Semáforos
**Sistemas Operativos - TP2**

Um servidor web HTTP/1.1 concorrente de nível de produção, implementando sincronização avançada de processos e threads utilizando semáforos POSIX, memória partilhada e thread pools.

## Índice
Visão Geral
Início Rápido
Funcionalidades
Estrutura do Projeto
Configuração
Testes
Detalhes de Implementação
Autores

## Visão Geral
Este projeto implementa um servidor web HTTP/1.1 multiprocesso e multi-thread que demonstra:
* **Gestão de Processos:** Arquitetura Mestre-Trabalhador (Master-Worker) usando `fork()`.
* **Comunicação Inter-Processos (IPC):** Memória partilhada e semáforos POSIX.
* **Sincronização de Threads:** Mutexes Pthread, variáveis de condição e trincos de leitura-escrita (reader-writer locks).
* **Tratamento de Pedidos Concorrentes:** Thread pools com padrão produtor-consumidor.
* **Protocolo HTTP:** Suporte HTTP/1.1 incluindo métodos GET e HEAD.
* **Gestão de Recursos:** Cache de ficheiros LRU thread-safe e rastreio de estatísticas.

## Início Rápido

### 1. Compilação
Compila o servidor usando o Make:
make clean && make

### 2. Executar o Servidor
Inicia o servidor na porta padrão (8080):
./server

### 3. Aceder
Browser: http://localhost:8080
Curl: curl -v http://localhost:8080/index.html


### 4. Funcionalidades
Funcionalidades Principais (Core)

    [x] Arquitetura Multi-Processo: 1 mestre + N trabalhadores.

    [x] Thread Pools: Padrão Produtor-Consumidor.

    [x] Suporte HTTP/1.1: Métodos GET e HEAD.

    [x] Códigos de Estado: 200, 404, 403, 500, 503.

    [x] Tipos MIME: HTML, CSS, JavaScript, imagens (PNG, JPG), PDF.

    [x] Páginas de Erro Personalizadas: Páginas 404 e 500 customizadas.

Funcionalidades de Sincronização

    [x] Semáforos POSIX: Sincronização entre processos.

    [x] Mutexes Pthread: Exclusão mútua ao nível da thread.

    [x] Variáveis de Condição: Sinalização da fila de conexões.

    [x] Trincos Leitura-Escrita (RW Locks): Acesso seguro à cache de ficheiros.

Funcionalidades Bónus

    [x] HTTP Keep-Alive: Conexões persistentes.

    [x] Dashboard em Tempo Real: Visualizador de estatísticas via Web.

    [ ] Suporte CGI: Execução de scripts dinâmicos.

    [x] Virtual Hosts: Múltiplos domínios num servidor.

    [ ] Suporte HTTPS: Encriptação SSL/TLS.


### 5. Estrutura do Projeto
src/
  main.c
  master.c/h
  worker.c/h
  http.c/h
  thread_pool.c/h
  cache.c/h
  logger.c/h
  stats.c/h
  config.c/h
www/
  index.html
  errors/
tests/
Makefile
server.conf
README.md

### 6. Testes

Fazer quando tivermos os testes

### 7. Detalhes de Implementação
Processo Mestre

    Inicializa o socket de escuta.

    Cria memória partilhada e semáforos.

    Faz fork de N processos trabalhadores.

    Aceita conexões e coloca-as na fila partilhada (Produtor).

Processos Trabalhadores

    Mantém uma thread pool de tamanho fixo.

    Threads retiram conexões da fila partilhada (Consumidor).

    Atualiza estatísticas partilhadas atomicamente.

    Faz cache de ficheiros em memória usando um algoritmo LRU protegido por trincos Leitura-Escrita.


### 8. Problemas Conhecidos
 
Fazer depois também

### Autores

    Martim Travesso Dias

        Nº Mec: 125925

        Email: martimtdias@ua.pt

    Nuno Carvalho Costa

        Nº Mec: 125120

        Email: nunoc27@ua.pt

