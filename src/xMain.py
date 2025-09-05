import serial
import asyncio
from serial.tools import list_ports

class Colors:
    BLACK   = '\033[30m'
    RED     = '\033[31m'
    GREEN   = '\033[32m'
    YELLOW  = '\033[33m'
    BLUE    = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN    = '\033[36m'
    WHITE   = '\033[37m'
    RESET   = '\033[0m'

def pick_serial_port():
    ports = [p for p in list_ports.comports() if "CP210" in p.description]
    if not ports:
        print("No CP210x serial devices found.")
        exit(1)
    print("CP210x serial devices:")
    for i, port in enumerate(ports):
        print(f"[{i}] {port.device} - {port.description}")
    idx = input("Select device number: ").strip()
    try:
        chosen = ports[int(idx)].device
    except Exception:
        print("Invalid selection.")
        exit(1)
    return chosen

async def read_serial_async(ser):
    loop = asyncio.get_event_loop()
    while True:
        # Use run_in_executor to avoid blocking event loop
        line = await loop.run_in_executor(None, ser.readline)
        line = line.decode(errors='ignore').strip()
        if line:
            if line.startswith("PING"):
                print(f"{Colors.MAGENTA}{line}{Colors.RESET}")
            elif line.startswith("CHAT"):
                print(f"{Colors.GREEN}{line}{Colors.RESET}")
            elif line.startswith("No connection"):
                print(f"{Colors.RED}{line}{Colors.RESET}")
            else:
                print(line)

async def main():
    port = pick_serial_port()
    ser = serial.Serial(port, 115200, timeout=1)
    serial_task = asyncio.create_task(read_serial_async(ser))
    try:
        while True:
            msg = await asyncio.get_event_loop().run_in_executor(None, input, "Send message: ")
            msg = msg.strip()
            if msg:
                ser.write((msg + '\n').encode())
    except KeyboardInterrupt:
        serial_task.cancel()
        ser.close()
        print("Serial connection closed.")

if __name__ == "__main__":
    asyncio.run(main())