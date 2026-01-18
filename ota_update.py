import socket
import os
import json
import time
import sys
import hashlib  # Додайте цей імпорт

ESP32_IP = "192.168.38.173"
PORT = 5000
FILENAME = "actuator_pwm.ino.bin" 

def send_update():
    if not os.path.exists(FILENAME):
        print(f"Помилка: Файл {FILENAME} не знайдено!")
        return

    # Читаємо файл та рахуємо MD5
    with open(FILENAME, "rb") as f:
        file_data = f.read()
        file_size = len(file_data)
        # ОСЬ ЦЕЙ РЯДОК: створюємо хеш вмісту файлу
        md5_hash = hashlib.md5(file_data).hexdigest()

    print(f"Розмір: {file_size} байт")
    print(f"MD5: {md5_hash}")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ESP32_IP, PORT))
            
            # Додаємо md5 у JSON пакет
            start_cmd = {
                "cmd": "ota_start", 
                "size": file_size, 
                "md5": md5_hash  # Надсилаємо хеш контролеру
            }
            
            s.sendall((json.dumps(start_cmd) + "\n").encode())
            print("Команду з MD5 надіслано. Очікування...")
            time.sleep(2) 
            
            # Передача файлу з невеликою затримкою для стабільності UART
            bytes_sent = 0
            chunk_size = 1024
            for i in range(0, len(file_data), chunk_size):
                chunk = file_data[i:i + chunk_size]
                s.sendall(chunk)
                bytes_sent += len(chunk)
                time.sleep(0.01) # 10мс пауза між КБ для CH9120
                sys.stdout.write(f"\rПрогрес: {(bytes_sent/file_size)*100:.1f}%")
                sys.stdout.flush()
            
            print("\nГотово!")
    except Exception as e:
        print(f"\nПомилка: {e}")

if __name__ == "__main__":
    send_update()