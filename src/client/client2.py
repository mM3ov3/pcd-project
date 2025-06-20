#!/usr/bin/env python3
import socket
import struct
import random
import threading
import time
import os

SERVER_IP = "127.0.0.1"
SERVER_PORT = 5555
HEARTBEAT_INTERVAL = 15
UDP_RETRIES = 3
UDP_TIMEOUT = 3

class VideoClient:
    def __init__(self, server_ip="127.0.0.1", server_port=5555):
        self.server_ip = server_ip
        self.server_port = server_port
        self.client_id = None
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(UDP_TIMEOUT)
        self.heartbeat_thread = None
        self.result_thread = None
        self.running = True
        self.jobs = {}

    def send_with_retry(self, packet):
     for attempt in range(UDP_RETRIES):
        self.sock.sendto(packet, (self.server_ip, self.server_port))
        try:
            data, _ = self.sock.recvfrom(1024)
            return data
        except socket.timeout:
            print(f"[WARN] Timeout, retry {attempt + 1}/{UDP_RETRIES}...")
        except Exception as e:
            print(f"[ERROR] Socket error: {e}")
            return None
     return None


    def send_client_id_req(self):
        msg_id = random.randint(1, 1_000_000)
        packet = struct.pack("!BI", 1, msg_id)
        data = self.send_with_retry(packet)
        if data and len(data) >= 21:
            recv_type, recv_msg_id = struct.unpack("!BI", data[:5])
            if recv_type == 2:
                self.client_id = data[5:21]
                print(f"[INFO] Received CLIENT ID: {self.client_id.hex()}")
                return True
        print("[ERROR] Failed to receive CLIENT_ID_ACK")
        return False

    def start_heartbeat(self):
        def loop():
            while self.running:
                try:
                    packet = struct.pack("!B16s", 2, self.client_id)
                    self.sock.sendto(packet, (self.server_ip, self.server_port))
                except Exception as e:
                    print(f"[HEARTBEAT] Error: {e}")
                time.sleep(HEARTBEAT_INTERVAL)
        self.heartbeat_thread = threading.Thread(target=loop, daemon=True)
        self.heartbeat_thread.start()

    def listen_for_results(self):
        while self.running:
            try:
                data, _ = self.sock.recvfrom(1024)
                if data[0] == 7 and len(data) >= 27:
                    job_id = struct.unpack("!I", data[21:25])[0]
                    status = data[25]
                    msg_len = data[26]
                    message = data[27:27+msg_len].decode()
                    print(f"[JOB_RESULT] Job {job_id} status={status}: {message}")
                    if job_id in self.jobs:
                        self.jobs[job_id]["status"] = "done"
            except socket.timeout:
                continue

    def start_result_listener(self):
        self.result_thread = threading.Thread(target=self.listen_for_results, daemon=True)
        self.result_thread.start()

    def create_job(self, job_id, command, file_count):
        msg_id = random.randint(1, 1_000_000)
        cmd_bytes = command.encode()
        packet = struct.pack(f"!BI16sIBH{len(cmd_bytes)}s",
                             3, msg_id, self.client_id, job_id, file_count,
                             len(cmd_bytes), cmd_bytes)
        data = self.send_with_retry(packet)
        if data and len(data) >= 11:
            recv_type, recv_msg_id, job_id_recv, status, msg_len = struct.unpack("!BII BB", data[:11])
            message = data[11:11+msg_len].decode()
            print(f"[INFO] Job ACK: {message}")
            if status == 1:
                self.jobs[job_id] = {"command": command, "status": "pending"}
                return True
        return False

    def send_upload_request(self, job_id, file_path):
        msg_id = random.randint(1, 1_000_000)
        file_size = os.path.getsize(file_path)
        filename = os.path.basename(file_path).encode()
        name_len = len(filename)

        packet = struct.pack(f"!BI16sIQB{name_len}s",
                             5, msg_id, self.client_id, job_id,
                             file_size, name_len, filename)
        data = self.send_with_retry(packet)
        if data and len(data) >= 10:
            recv_type, recv_msg_id, name_len = struct.unpack("!BIB", data[:6])
            offset = 6
            file_name = data[offset:offset+name_len].decode()
            offset += name_len
            status, ip_raw, port = struct.unpack("!B I H", data[offset:offset+7])
            ip = socket.inet_ntoa(struct.pack("!I", ip_raw))
            print(f"[INFO] Upload ACK: {file_name} to {ip}:{port}")
            return status == 1, ip, port
        return False, None, None

    def send_file_tcp(self, ip, port, job_id, file_path):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as tcp_sock:
                tcp_sock.connect((ip, port))
                file_size = os.path.getsize(file_path)
                filename = os.path.basename(file_path).encode()

                header = struct.pack(f"!16sI I{len(filename)}s Q",
                                     self.client_id, job_id,
                                     len(filename), filename, file_size)
                tcp_sock.sendall(header)

                with open(file_path, "rb") as f:
                    while chunk := f.read(4096):
                        tcp_sock.sendall(chunk)
            print("[INFO] File uploaded successfully.")
            return True
        except Exception as e:
            print(f"[ERROR] TCP upload failed: {e}")
            return False

    def send_download_request(self, job_id, filename):
        msg_id = random.randint(1, 1_000_000)
        fname_bytes = filename.encode()
        packet = struct.pack(f"!BI16sI B{len(fname_bytes)}s",
                             6, msg_id, self.client_id, job_id,
                             len(fname_bytes), fname_bytes)
        data = self.send_with_retry(packet)
        if data and len(data) >= 10:
            recv_type, recv_msg_id, name_len = struct.unpack("!BIB", data[:6])
            offset = 6
            recv_fname = data[offset:offset+name_len].decode()
            offset += name_len
            status, ip_raw, port = struct.unpack("!B I H", data[offset:offset+7])
            ip = socket.inet_ntoa(struct.pack("!I", ip_raw))
            print(f"[INFO] Download ACK: {recv_fname} from {ip}:{port}")
            return status == 1, ip, port, recv_fname
        return False, None, None, None

    def receive_file_tcp(self, ip, port, filename):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as tcp_sock:
                tcp_sock.connect((ip, port))
                with open(filename, "wb") as f:
                    while True:
                        data = tcp_sock.recv(4096)
                        if not data:
                            break
                        f.write(data)
            print("[INFO] File downloaded successfully.")
            return True
        except Exception as e:
            print(f"[ERROR] TCP download failed: {e}")
            return False

    def shutdown(self):
        self.running = False
        if self.heartbeat_thread:
            self.heartbeat_thread.join(timeout=1)
        if self.result_thread:
            self.result_thread.join(timeout=1)
        self.sock.close()

def display_menu():
    print("=============================")
    print("        Video Options        ")
    print("=============================")
    print(" 1. Trim video")
    print(" 2. Resize video")
    print(" 3. Convert format")
    print(" 4. Extract audio")
    print(" 5. Extract video")
    print(" 6. Adjust brightness")
    print(" 7. Adjust contrast")
    print(" 8. Adjust saturation")
    print(" 9. Rotate video")
    print("10. Crop video")
    print("11. Add watermark")
    print("12. Add subtitles")
    print("13. Change playback speed")
    print("14. Reverse video")
    print("15. Extract frame")
    print("16. Create GIF")
    print("17. Denoise video")
    print("18. Stabilize video")
    print("19. Merge videos")
    print("20. Add audio track")
    print(" 0. Exit")
    print("=============================")

def generate_command(choice, input_file, output_file):
    match choice:
        case 1:  return f"ffmpeg -i {input_file} -ss 00:00:05 -t 00:00:10 -c copy {output_file}"
        case 2:  return f"ffmpeg -i {input_file} -vf scale=1280:720 {output_file}"
        case 3:  return f"ffmpeg -i {input_file} {output_file}"
        case 4:  return f"ffmpeg -i {input_file} -q:a 0 -map a {output_file}"
        case 5:  return f"ffmpeg -i {input_file} -an {output_file}"
        case 6:  return f"ffmpeg -i {input_file} -vf eq=brightness=0.06 {output_file}"
        case 7:  return f"ffmpeg -i {input_file} -vf eq=contrast=1.5 {output_file}"
        case 8:  return f"ffmpeg -i {input_file} -vf eq=saturation=2.0 {output_file}"
        case 9:  return f"ffmpeg -i {input_file} -vf transpose=1 {output_file}"
        case 10: return f"ffmpeg -i {input_file} -filter:v \"crop=300:300:100:100\" {output_file}"
        case 11: return f"ffmpeg -i {input_file} -i watermark.png -filter_complex \"overlay=10:10\" {output_file}"
        case 12: return f"ffmpeg -i {input_file} -vf subtitles=subs.srt {output_file}"
        case 13: return f"ffmpeg -i {input_file} -filter:v \"setpts=0.5*PTS\" {output_file}"
        case 14: return f"ffmpeg -i {input_file} -vf reverse -af areverse {output_file}"
        case 15: return f"ffmpeg -i {input_file} -ss 00:00:01.000 -vframes 1 {output_file}"
        case 16: return f"ffmpeg -i {input_file} -vf \"fps=10,scale=320:-1:flags=lanczos\" -t 5 {output_file}"
        case 17: return f"ffmpeg -i {input_file} -vf hqdn3d {output_file}"
        case 18: return f"ffmpeg -i {input_file} -vf vidstabdetect=shakiness=5:accuracy=15 -f null - && ffmpeg -i {input_file} -vf vidstabtransform=smoothing=30 {output_file}"
        case 19: return f"ffmpeg -i \"concat:file1.mp4|file2.mp4\" -c copy {output_file}"
        case 20: return f"ffmpeg -i {input_file} -i audio.mp3 -c:v copy -map 0:v:0 -map 1:a:0 {output_file}"
        case _:  return None

def main():
    client = VideoClient()

    if not client.send_client_id_req():
        print(" Failed to obtain client ID.")
        return

    client.start_heartbeat()
    client.start_result_listener()

    while True:
        display_menu()
        try:
            choice = int(input("Enter your choice: "))
        except ValueError:
            print("Invalid input. Try again.")
            continue

        if choice == 0:
            client.shutdown()
            break

        if 1 <= choice <= 20:
            input_file = input("Input file name: ").strip()
            output_file = input("Output file name: ").strip()

            command = generate_command(choice, input_file, output_file)
            job_id = random.randint(1000, 9999)

            if client.create_job(job_id, command, 1):
                ok, ip, port = client.send_upload_request(job_id, input_file)
                if ok:
                    client.send_file_tcp(ip, port, job_id, input_file)
                    time.sleep(2)  
                    ok, ip, port, name = client.send_download_request(job_id, output_file)
                    if ok:
                        client.receive_file_tcp(ip, port, name)
        elif choice == 21:
            job_id = int(input("Job ID: "))
            filename = input("Filename to download: ")
            ok, ip, port, name = client.send_download_request(job_id, filename)
            if ok:
                client.receive_file_tcp(ip, port, name)

if __name__ == "__main__":
    main()
