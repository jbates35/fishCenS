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
