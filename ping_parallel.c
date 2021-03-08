#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include "linkedlist.h"

#define TOTAL_IP    100000
#define REQUEST_RETRIES 1

struct ip_data_t{
    char ip_s[16];
    struct timeval timestamp;
    int retries;
};

pthread_mutex_t m;
sem_t prod_s;

/* Lista de IPs Paralelos. */ 
struct linkedlist_t *ip_list;

/* Produtor
 * Gera IPs classe A e coloca em uma lista.
 */
void* genHostIP(void * p){
    int cnt = TOTAL_IP;
    char ip_s[INET_ADDRSTRLEN];
    struct ip_data_t ipdt;

    memset(&ipdt, 0, sizeof(struct ip_data_t));

    while(cnt--){
        sprintf(ipdt.ip_s, "%d.%d.%d.%d", rand()%127, rand()%256, rand()%256, rand()%256);

        printf("IP gerado -> %s\n", ipdt.ip_s);
        fflush(stdout);

        /* Insere IP na list */
        sem_wait(&prod_s);
        pthread_mutex_lock(&m);

        linkedlist_insert_tail(ip_list, &ipdt);

        pthread_mutex_unlock(&m);
    }  
}

/* Consumidor
 * Envia ping para os IPs da lista e aguarda resposta.
 */
void * checkHost(void *p){
    int fd;
    struct linkedlist_node_t *node;
    struct icmphdr req;
    const size_t req_size=8;
    struct sockaddr_in sin, cliaddr;
    struct ip_data_t *ipdt;
    struct timeval tm;
    int n, len;
    char ip_s[INET_ADDRSTRLEN];
    char msg[64];

    /* Cria socket ICMP */
    if ( (fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)) < 0){
        perror("socket");
        fflush(stdout);
    }

    /* Configura timeout para recepção de pacotes 
     * Se o recvfrom() não receber nada conforme o tempo configurado,
     * retorna com timeout.
    */
    struct timeval read_timeout;
    read_timeout.tv_sec = 1;
    read_timeout.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0){
        perror("setsockopt");
        fflush(stdout);
    }

    /* Configura cabeçalho ICMP. */
    memset(&req, 0, sizeof(req));
    req.type=ICMP_ECHO;
    req.code=0;
    req.checksum=0;
    req.un.echo.sequence=htons(1);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    while(1){
        
        pthread_mutex_lock(&m);
        
        node = ip_list->first;
        /* Percorre a lista de IPs enviando ping requests e removendo IPs
         * com expirados. 
        */
        while(node){

            node = get_next(node, (void*)&ipdt);

            /* tentativas esgotadas sem resposta */
            if(ipdt->retries >= REQUEST_RETRIES){
                printf("%s não responde.\n", ipdt->ip_s);
                linkedlist_remove(ip_list, ipdt);
                sem_post(&prod_s);
            }else{
                
                /* Realiza nova tentativa */ 
                gettimeofday(&tm, NULL);
                if ( (tm.tv_sec - ipdt->timestamp.tv_sec) > 1){
                    if (inet_pton(AF_INET, ipdt->ip_s, &sin.sin_addr) <= 0){
                        perror("inet_pton");
                        fflush(stdout);
                    }

                    ipdt->retries++;

                    printf("%s -> tentativa %d\n", ipdt->ip_s, ipdt->retries);
                    fflush(stdout);
                    if (sendto(fd, &req, req_size, 0, (struct sockaddr *)&sin, sizeof(sin))==-1) {
                        perror("sendto");
                    }

                    gettimeofday(&ipdt->timestamp, NULL);
                }
            }
        }
        pthread_mutex_unlock(&m);

        /* Aguarda recebimento de ping responses. */
        n = 1;
        while(n > 0){
            len = sizeof(cliaddr);
            if ((n = recvfrom(fd, (char *)msg, sizeof(msg), MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len)) <= 0){
                break;
            }

            struct icmphdr rcv_hdr;
            memcpy(&rcv_hdr, msg, sizeof(rcv_hdr));
            
            if (rcv_hdr.type == ICMP_ECHOREPLY) {
                inet_ntop(AF_INET, &(cliaddr.sin_addr), ip_s, INET_ADDRSTRLEN);

                printf("%s respondeu.\n", ip_s);

                pthread_mutex_lock(&m);

                node = ip_list->first;

                while(node){
                    node = get_next(node, (void*)&ipdt);
                    if (!strcmp(ip_s, ipdt->ip_s)){
                        linkedlist_remove(ip_list, ipdt);
                        sem_post(&prod_s);
                        break;
                    }
                }
                pthread_mutex_unlock(&m);

            }

        }
    }
}

int main(int argc, char**argv){
    pthread_t prod, cons;
    int parallel_ips;


    if (argc < 2){
        printf("usage: %s <parallel ips>\n", argv[0]);
        return 0;
    }

    parallel_ips = atoi(argv[1]);

    srand(time(NULL));

    ip_list = linkedlist_create(sizeof(struct ip_data_t));

    pthread_mutex_init(&m, NULL);
    sem_init(&prod_s, 0, parallel_ips);

    /* Cria uma thread produtora */
    if(pthread_create(&prod, NULL, genHostIP, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    /* Cria uma thread produtora */
    if(pthread_create(&cons, NULL, checkHost, NULL)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    /* Aguarda o término da thread produtora */
    if(pthread_join(prod, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        return 3;
    }

    /* Aguarda o esvaziamento do buffer. */
    while (linkedlist_size(ip_list) > 0){
        sleep(1);
    }   

    return 0;
}