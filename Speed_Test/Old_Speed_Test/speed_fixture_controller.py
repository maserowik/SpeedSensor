# Modernized speed_fixture_controller.py
# Original functionality preserved; improved readability, logging, and buffer management
# Author: Drew Bermudez | Modernized 2025

import time
import random
import serial
import logging
from bluetooth import BluetoothSocket, RFCOMM, advertise_service, SERIAL_PORT_CLASS, SERIAL_PORT_PROFILE
from RPLCD.i2c import CharLCD

# -------------------------------
# Setup logging
# -------------------------------
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)

# -------------------------------
# Bluetooth communication class
# -------------------------------
class TabletBtComms:
    """Handles simple Bluetooth communication with the tablet."""
    
    def __init__(self):
        self.serverSocket = BluetoothSocket(RFCOMM)
        self.serverSocket.bind(("", 0))
        self.serverSocket.listen(1)
        self.port = self.serverSocket.getsockname()[1]
        self.uuid = "94f39d29-7d6d-437d-973b-fba39e49d4ee"
        advertise_service(
            self.serverSocket,
            "SeegridPi",
            service_id=self.uuid,
            service_classes=[self.uuid, SERIAL_PORT_CLASS],
            profiles=[SERIAL_PORT_PROFILE]
        )
        self.clientSocket = None
        self.clientInfo = None
        self.btReadings = []
    
    def connect(self):
        logging.info("Waiting for Bluetooth connection...")
        self.clientSocket, self.clientInfo = self.serverSocket.accept()
        self.clientSocket.setblocking(False)
        logging.info(f"Accepted connection from {self.clientInfo}")
    
    def update(self):
        try:
            data = self.clientSocket.recv(1024)
            if data:
                self.btReadings.append(data.decode("utf-8"))
        except:
            pass  # Non-blocking read; ignore if no data
    
    def send(self, message: str):
        try:
            self.clientSocket.send(message)
        except Exception as e:
            logging.warning(f"Failed to send message via Bluetooth: {e}")


# -------------------------------
# LCD display class
# -------------------------------
class TestFixtureLCD(CharLCD):
    """LCD display with built-in commands for test fixture."""
    
    def __init__(self, lcdID, location, rows=4, cols=20, backlight_enabled=True):
        super().__init__(lcdID, location, rows=rows, cols=cols, backlight_enabled=backlight_enabled)
        # Custom character for Bluetooth symbol
        bluetooth_symbol = (
            0b01000,
            0b01100,
            0b11010,
            0b01100,
            0b01100,
            0b11010,
            0b01100,
            0b01000
        )
        self.create_char(3, bluetooth_symbol)
    
    def display_bluetooth_connected(self):
        self.clear()
        self.write_string('Bluetooth Connected\n\n\t\t\t\t\x03')


# -------------------------------
# Arduino serial communication class
# -------------------------------
class ArduinoSerial:
    """Handles serial connection to Arduino."""
    
    def __init__(self, baudRate=9600, debug=False):
        self.baudRate = baudRate
        self.debug = debug
        self.serialReadings = []
        self.isSerialConnected = False
        self.connect_serial()
    
    def connect_serial(self):
        """Attempt to connect to available Arduino serial ports."""
        self.serialConnection = None
        ports = ['/dev/ttyACM0', '/dev/ttyACM1', '/dev/ttyACM2', '/dev/ttyACM3']
        for port in ports:
            try:
                self.serialConnection = serial.Serial(port, self.baudRate, timeout=0.1)
                self.isSerialConnected = True
                if self.debug:
                    logging.info(f"Connected to Arduino on {port}")
                break
            except:
                continue
        if not self.isSerialConnected:
            logging.error("ERROR CONNECTING TO SERIAL PORT")
    
    def update(self):
        """Read all available serial data into buffer."""
        if self.serialConnection and self.serialConnection.in_waiting > 0:
            lines = self.serialConnection.readlines()
            for line in lines:
                self.serialReadings.append(line.decode("utf-8").strip())
    
    def send(self, command: str):
        """Send command to Arduino."""
        try:
            if self.serialConnection:
                self.serialConnection.write(command.encode())
                if self.debug:
                    logging.info(f"WRITING TO ARDUINO: {command}")
        except Exception as e:
            logging.warning(f"Failed to write to Arduino: {e}")


# -------------------------------
# Main fixture loop
# -------------------------------
def run_fixture():
    btConnection = TabletBtComms()
    lcd = TestFixtureLCD('PCF8574', 0x27)
    lcd.write_string('Connect Bluetooth')
    
    btConnection.connect()
    lcd.display_bluetooth_connected()
    time.sleep(1)  # Pause so user sees Bluetooth connection

    feed = False
    lastNoReadingTime = time.time() + 1
    noReadingCommInterval = 1
    arduinoSerialConnection = ArduinoSerial(9600)
    
    # Function to simulate speed measurement
    def get_speed_str():
        randomBreak = random.randint(0, 2)
        randomSpeed = random.random() * 10
        if randomBreak == 0:
            return "!N"
        return f"!S {randomSpeed}"

    # Main loop
    while True:
        speed = get_speed_str()  # Simulated speed (replace with real Arduino data if needed)
        btConnection.update()
        arduinoSerialConnection.update()

        # Periodic communication
        if (time.time() - lastNoReadingTime) > noReadingCommInterval:
            lastNoReadingTime = time.time()
            # Check for tablet commands
            if btConnection.btReadings:
                tabletCommand = btConnection.btReadings.pop(0)
                if tabletCommand == "t":
                    feed = True

            if feed and arduinoSerialConnection.serialReadings:
                data = arduinoSerialConnection.serialReadings.pop(0)
                if data and len(data) > 1 and data[1] != "N":
                    lcd.clear()
                    lcd.write_string(data[2:])
                    btConnection.send(data)
            else:
                btConnection.send("!N")


# -------------------------------
# Entry point
# -------------------------------
def main():
    run_fixture()
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())
