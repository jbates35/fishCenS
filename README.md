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