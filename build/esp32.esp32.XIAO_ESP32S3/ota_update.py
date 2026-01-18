import socket
import os
import json
import time
import sys
import hashlib

ESP32_IP = "192.168.38.173"
PORT = 5000
FILENAME = "actuator_pwm.ino.bin"
BLOCK_SIZE = 512 

def send_update():
    file_data = open(FILENAME, "rb").read()
    file_size = len(file_data)
    md5_hash = hashlib.md5(file_data).hexdigest().lower()

    print(f"[*] Файл: {FILENAME} ({file_size} байт)")
    print(f"[*] MD5: {md5_hash}")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(15) # Трохи збільшимо для стабільності
            s.connect((ESP32_IP, PORT))
            
            # 1. Відправка команди старту
            start_cmd = {"cmd": "ota_start", "size": file_size, "md5": md5_hash}
            s.sendall((json.dumps(start_cmd) + "\n").encode())
            
            # 2. Очікування готовності ESP32
            print("[Python] Чекаю RES:READY...")
            resp = s.recv(1024).decode(errors='ignore')
            if "RES:READY" not in resp:
                print(f"[Python] Помилка старту: {resp}")
                return
            
            print("[Python] Отримано RES:READY. Починаю синхронну передачу пакетів...")

            bytes_sent = 0
            packet_num = 0
            total_size = len(file_data)
            
            # 3. ОСНОВНИЙ ЦИКЛ ПЕРЕДАЧІ (один цикл!)
            for i in range(0, total_size, BLOCK_SIZE):
                chunk = file_data[i : i + BLOCK_SIZE]
                s.sendall(chunk)
                packet_num += 1
 
                # ОЧІКУВАННЯ ПІДТВЕРДЖЕННЯ (Handshake)
                # ESP32 має відправити RES:OK_PKT_X після кожного BLOCK_SIZE
                try:
                    s.settimeout(5.0) # Flash erase може бути довгим
                    data = b""
                    while b"\n" not in data:
                        char = s.recv(1)
                        if not char: break
                        data += char
                    
                    response = data.decode().strip()
                    # Можна розкоментувати для налагодження:
                    # print(f"Пакет {packet_num}: {response}")
                    
                    if "RES:OK_PKT" not in response:
                        print(f"\n[!] Отримано неочікувану відповідь: {resp}")
                        if "RES:ERR" in resp: return

                    bytes_sent += len(chunk)
                    percent = (bytes_sent / file_size) * 100
                    sys.stdout.write(f"\r[>] Прогрес: {percent:.1f}% ({bytes_sent}/{file_size})")
                    sys.stdout.flush()

                except socket.timeout:
                    print(f"\n[Python] ТАЙМАУТ на пакеті {packet_num} ({bytes_sent} байт)")
                    return

            print("\n[Python] Передача завершена. Чекаю фінальну відповідь...")
            # Чекаємо RES:DONE або помилку MD5
            s.settimeout(20.0) 
            print(s.recv(1024).decode(errors='ignore'))

    except Exception as e:
        print(f"\n[Python] КРИТИЧНА ПОМИЛКА: {e}")
        
if __name__ == "__main__":
    send_update()