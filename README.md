# fishCenS
A fish-counting system that can be used to track salmon as they swim upstream creeks

Some things that need to be installed first:

- Follow this guide for OpenCV. 4.5 or higher is optimal: https://qengineering.eu/install-opencv-4.5-on-raspberry-pi-4.html
- Follow this guide for installing libcamera: https://forums.raspberrypi.com/viewtopic.php?t=324997 (See bluePiuser's post) ... Tom you might need to edit this?

CMAKE:
After cloning the repo:
- Make a folder in the fishCenS folder called build.
- Tell cmake to generate a makefile to build the project
- Then make the project and run.

The commands look like this:

```
cd ~
git clone https://github.com/jbates35/fishCenS
cd fishCenS
mkdir build
cd build
cmake ..
make
```

You'll likely want to make your own branch (please for the love of god do):

```
cd ~/fishCenS/
git checkout -b "myNewBranch"
```

After doing so, now you can work on your own code, add files, whatever. Once you're done, you need to commit your files, and then push
```
git add . 
git commit -m "My commit description goes here"
git push
```

After coming back the next day, there may be updates on the remote repository (i.e. on github). Do:
```
git fetch
```
to see if there are.

If you want to integrate the changes into what you're working on, feel free to use
```
git pull
```
Be careful, make sure you commit all your changes you want to keep before using git pull.

In the cmake file, whenever you add a new file, you want to add it here:
![image](https://user-images.githubusercontent.com/70033294/218302179-2f91b61e-8081-466a-b324-c7d3f3a45e21.png)

If you prefer working in codeblocks, refer to this stackoverflow page on how to do so:
https://stackoverflow.com/questions/37618040/how-to-create-a-codeblocks-project-from-the-cmake-file

TEMPERATURE SENSOR.

On Raspberry Pi go Preferances -> Raspberry Pi Configuration -> Interfaces 
and toggle on 1-Wire

Connect the sensors's -V wire to a ground pin and the +V to the 3.3V supply
on the Pi.
By default Pi's 1-wire pin is pin #4 or pin #7 if you're using the physical 
numbering.
You also need to connect the 1-wire pin to the 3.3V supply with a 4.7 kohm resistor

The code has the address of this temperature sensor hardcoded, but if you
need to add a temp sensor or use a different one you can get the name of the
connected sensor by using commands to navigate to /sys/bus/w1/devices and 
using the ls command. The sensor will appear as 28-0xxxxxxxxxxx. Replace
this number with the written in the code.

ULTRASONIC.

On the Pi go to Preferences -> Raspberry Pi Configurations -> Interfaces
and toggle on Serial Port.

The snesor needs a 5V supply, ground and connect the sensor output to GPIO 15 (physical pin 10).

The default serial pin uses a miniUart module which can be unstable
when the pi is under heavy load, so we will change the pin to connect to
the PL011 serial modual which is typically used for Bluetooth.

Edit the Pi's Config file with:
```
sudo nano /boot/config.txt
```
And add the following line to the end of the file:
```
dtoverlay = pi3-miniuart-bt
```
Save the file and reboot, you can check the connections with:
```
ls -l /dev
```
this should show

serial0 -> tty0AMA

serial1 -> ttyS0


PIN/Colors:
Ultrasonic sensor: https://wiki.dfrobot.com/_A02YYUW_Waterproof_Ultrasonic_Sensor_SKU_SEN0311
Note: Signals are from perspective of sensor
```
Vcc         -> Red/Red
Gnd         -> Black/Black
RX (Val)    -> Yellow/Blue  
TX          -> White/Green
```

Temperature sensor: https://www.sparkfun.com/products/11050
```
Vcc   -> Red
Gnd   -> Black
Sig   -> White
```
Note Temperature sensor VCC can be 3.3V or 5V
