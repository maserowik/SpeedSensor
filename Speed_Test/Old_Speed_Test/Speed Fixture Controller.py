
#Drew Bermudez | Seegrid | 2018
import os
import glob
import time
import random
import serial
from RPLCD.i2c import CharLCD
from bluetooth import *



class TabletBtComms():
	"""Class connects to bluetooth and provides extremely simple interfacing 
	between rpi and tablet."""
	
	def __init__(self):
		self.serverSocket = BluetoothSocket( RFCOMM )
		self.serverSocket.bind(("",PORT_ANY))
		self.serverSocket.listen(1)

		self.port = self.serverSocket.getsockname()[1]

		self.uuid = "94f39d29-7d6d-437d-973b-fba39e49d4ee"

		advertise_service( self.serverSocket, "SeegridPi",
						   service_id = self.uuid,
						   service_classes = [ self.uuid, SERIAL_PORT_CLASS ],
						   profiles = [ SERIAL_PORT_PROFILE ],
							)	              
		self.receivedData=""
		self.btReadings=[]
		
	def connect(self):
		#BLOCKING - waits for bluetooth device to connect to RPI.
		self.clientSocket, self.clientInfo = self.serverSocket.accept()
		self.clientSocket.setblocking(False) #This enables bluetooth updates to not be blocking.
		
	def update(self):
		try:
			self.receivedData = self.clientSocket.recv(1024)
			self.btReadings.append(self.receivedData.decode("utf-8"))
		except:
			pass
	def send(self,message):
		self.clientSocket.send(message)
			
class TestFixtureLCD(CharLCD):
	"""Class to connect to LCD with built in commands for standard test fixture displays"""
	
	def __init__(self,lcdID,location,rows=4,cols=20,backlight_enabled=True):
		super().__init__(lcdID,location,rows=rows,cols=cols,backlight_enabled=backlight_enabled)
		self.bluetoothSymbol = (
		0b01000,
		0b01100,
		0b11010,
		0b01100,
		0b01100,
		0b11010,
		0b01100,
		0b01000
		)
		self.create_char(3,self.bluetoothSymbol)
		
	def displayBluetoothConnected(self):
		self.clear()
		self.write_string('Bluetooth Connected')
		self.write_string('\n')
		self.write_string('\n\t\t\t\t                \x03') 


class ArduinoSerial:
	"""Class to establish simple serial connection to arduino."""
	
	def __init__(self,baudRate=9600,debug=False):
		self.baudRate=baudRate
		self.debug=debug
		self.serialReadings=[]
		self.isSerialConnected = False
		self.connectSerial()
		
	def connectSerial(self):
		#Sets up Serial data finding the proper Serial port
		#Can only be run once.
		self.serialConnection = None #initializing serial feed 
		self.isSerialConnected = True
		baudRate=self.baudRate
		try:
			self.serialConnection= serial.Serial('/dev/ttyACM0',baudRate)
		except:
			try:
				self.serialConnection= serial.Serial('/dev/ttyACM1',baudRate)
			except:
				self.serialConnection= serial.Serial('/dev/ttyACM2',baudRate)
			try:
				self.serialConnection= serial.Serial('/dev/ttyACM3',baudRate)
			except:
				serialConnected=False
				if self.debug:
					print("ERROR CONNECTING TO SERIAL PORT")
		if self.debug:
			print("connected")
		
	def update(self):
		##Updates a list of serial data
		if self.serialConnection.inWaiting()>0:

			self.serialReadings.append(self.serialConnection.readline().decode("utf-8"))
		
	def send(self,command):
		#encodes and sends command to serial
		try:
			self.serialConnection.write(command.encode())
			if self.debug:
				print("WRITING: " + command)     
		except:
			pass
			
def runFixture():			               
	btConnection = TabletBtComms()           
	lcd = TestFixtureLCD('PCF8574',0x27)
	lcd.write_string('Connect Bluetooth')
	print("Waiting for Bluetooth connection...")
	btConnection.connect()
	print("Accepted connection from ", btConnection.clientInfo)
	lcd.displayBluetoothConnected()
	time.sleep(1) #So the user can see that bluetooth has been connected.



	def getSpeedStr():
		randomBreak = random.randint(0,2)
		randomSpeed = random.random()*10
		if(randomBreak==0):
			return "!N"
		return "!S "+str(randomSpeed) #values in m/s


	#Main Loop
	feed=False
	lastNoReadingTime=time.time()+1
	noReadingCommInterval=1
	arduinoSerialConnection = ArduinoSerial(9600)
	
	while True:
		speed = getSpeedStr()   
		btConnection.update()
		arduinoSerialConnection.update()

		if (time.time()-lastNoReadingTime)>noReadingCommInterval:
			lastNoReadingTime=time.time()
			if(len(btConnection.btReadings)>0):
				tabletCommand = btConnection.btReadings.pop()
				if tabletCommand=="t":
					feed = True
			if feed and len(arduinoSerialConnection.serialReadings)>0:
				data = arduinoSerialConnection.serialReadings.pop()[:-1]
				if data!=None and data[1]!="N":
					lcd.clear()
					lcd.write_string(data[2:])

					btConnection.send(data)
			else:
				btConnection.send("!N")


def main(args):
	runFixture()
	return 0 
	
if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
