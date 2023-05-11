# fishCenS

A fish-counting system that can be used to track salmon as they swim upstream creeks

----------------------------

Once cloned, run the command
```bash
$ sudo bash installFishCenS.sh
```

Hopefully I remembered everything.

---------------------------

Some things that need to be installed first:

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

-------------

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

--------------

ULTRASONIC.

On the Pi go to Preferences -> Raspberry Pi Configurations -> Interfaces
and toggle on Serial Port.

The sensor needs a 3.3V supply, ground and connect the sensor output to GPIO 15 (physical pin 10).

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
RX (Select) -> Yellow/Blue  
TX (Val)    -> White/Green
```
*Note Ultrasonic sensor VCC can be 3.3V or 5V*

Temperature sensor: https://www.sparkfun.com/products/11050

```
*Note Temperature sensor VCC needs to be be 3.3V*

### You will have to add models, labels, and possibly vids to the py folder

Next part coming up soon...
Hint: See GDrive (and the function that allows you to DL gdrive links)
```
------------

https://qengineering.eu/install-opencv-4.5-on-raspberry-64-os.html
This guide is used to install opencv

You'll likely need to follow the memory swap lines:
$ sudo nano /usr/bin/zram.sh

write:
	mem=$(( ($totalmem / $cores)* 1024 * 3))

$ sudo reboot

Then run the installFishCenS.txt script.
Then:


$ sudo nano /etc/dphys-swapfile

write:
	set CONF_SWAPSIZE=100 with the Nano text editor

$ sudo reboot

------------

Note that there are two systemctls:

fishcens.service
fcserver.service

I just assumed you installed the program in the "~" folder, with the username "pi".
Just make sure the scripts link to the correct file. It should be clear which folder paths need to be changed.

Systemctl files tell the OS what to do when you boot the Pi, or other scenarios that happen during an OS's runtime. There are a couple commands necessary for using systemctl files:

- sudo systemctl enable fishcens.service
This will tell the computer to run this script on startup

- sudo systemctl disable fishcens.service
This will tell the computer to stop running this script on startup

- sudo systemctl start fishcens.service
This will run the script, i.e., start the program

- sudo systemctl stop fishcens.service
This will stop the script, i.e. stop the program

When deploying, you'll want to enable both fishcens.service and fcserver. I have not enabled these with the shell script. If you don't know they're going on, they can be quite heavy on the Pi and can be confusing when you go to test the main program and it can't open the camera.

------------

You will have to configure the NGINX server:

$ cd /etc/nginx/sites-available/
$ sudo nano default

Then change the script to reflect something like this:

```
server {
        listen 80 default_server;
        listen [::]:80 default_server;

        root /home/pi/fcServer;

        server_name _;

        location / {
                proxy_pass http://localhost:5000;
                proxy_set_header Host $host;
                proxy_set_header X-Real-IP $remote_addr;
        }

        location /fishPictures/ {
                alias /home/pi/fishCenS/build/fishPictures/;
        }
}
```

Check the veracity of:

$ sudo nginx -t

Then restart the nginx server:

$ sudo systemctl restart nginx

NGINX is a server program for your machine (think Apache). What editing the default file does is tells NGINX to load the Python flask proxy (i.e. the website) when the IP of the Pi is put into the browser of a computer connected to the same network (i.e. connected to the router the Pi is also on).

------------

I hope the google drive download link worked. But if not, here's the link: https://drive.google.com/drive/folders/1xp-MMvOHN0dpLNeSs2ASOwO7C8mZ74f9?usp=share_link

------------

Follow this to install the RTC module:

https://pimylifeup.com/raspberry-pi-rtc/

Vcc   -> Red
Gnd   -> Black
Sig   -> White

