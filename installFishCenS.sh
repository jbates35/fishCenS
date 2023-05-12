#!/bin/bash

# Store current directory in variable
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# From https://qengineering.eu/install-opencv-4.5-on-raspberry-64-os.html

cd ~

apt update
apt upgrade

# dependencies
apt-get -y install build-essential cmake git unzip pkg-config
apt-get -y install libjpeg-dev libpng-dev
apt-get -y install libavcodec-dev libavformat-dev libswscale-dev
apt-get -y install libgtk2.0-dev libcanberra-gtk* libgtk-3-dev
apt-get -y install libgstreamer1.0-dev gstreamer1.0-gtk3
apt-get -y  install libgstreamer-plugins-base1.0-dev gstreamer1.0-gl
apt-get -y install libxvidcore-dev libx264-dev
apt-get -y install python3-dev python3-numpy python3-pip
apt-get -y install libtbb2 libtbb-dev libdc1394-22-dev
apt-get -y install libv4l-dev v4l-utils
apt-get -y install libopenblas-dev libatlas-base-dev libblas-dev
apt-get -y install liblapack-dev gfortran libhdf5-dev
apt-get -y  install libprotobuf-dev libgoogle-glog-dev libgflags-dev
apt-get -y install protobuf-compiler

wget -O opencv.zip https://github.com/opencv/opencv/archive/4.5.5.zip
wget -O opencv_contrib.zip https://github.com/opencv/opencv_contrib/archive/4.5.5.zip
unzip opencv.zip
unzip opencv_contrib.zip
mv opencv-4.5.5 opencv
mv opencv_contrib-4.5.5 opencv_contrib
rm opencv.zip
rm opencv_contrib.zip

cd ~/opencv
mkdir build
cd build

cmake -D CMAKE_BUILD_TYPE=RELEASE \
-D CMAKE_INSTALL_PREFIX=/usr/local \
-D OPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules \
-D ENABLE_NEON=ON \
-D WITH_OPENMP=ON \
-D WITH_OPENCL=OFF \
-D BUILD_TIFF=ON \
-D WITH_FFMPEG=ON \
-D WITH_TBB=ON \
-D BUILD_TBB=ON \
-D WITH_GSTREAMER=ON \
-D BUILD_TESTS=OFF \
-D WITH_EIGEN=OFF \
-D WITH_V4L=ON \
-D WITH_LIBV4L=ON \
-D WITH_VTK=OFF \
-D WITH_QT=OFF \
-D OPENCV_ENABLE_NONFREE=ON \
-D INSTALL_C_EXAMPLES=OFF \
-D INSTALL_PYTHON_EXAMPLES=OFF \
-D PYTHON3_PACKAGES_PATH=/usr/lib/python3/dist-packages \
-D OPENCV_GENERATE_PKGCONFIG=ON \
-D BUILD_EXAMPLES=OFF ..

make -j4

make install
ldconfig
make clean
apt update

rm -rf ~/opencv
rm -rf ~/opencv_contrib

#Reinstall libcamera
apt remove -y libcamera*
apt install -y python3-pip git
pip3 install jinja2

apt install -y libboost-dev
apt install -y libgnutls28-dev openssl libtiff5-dev
apt install -y qtbase5-dev libqt5core5a libqt5gui5 libqt5widgets5
apt install -y meson
pip3 install pyyaml ply
pip3 install --upgrade meson
apt install -y libglib2.0-dev libgstreamer-plugins-base1.0-dev
cd
git clone git://linuxtv.org/libcamera.git
cd libcamera
meson build --buildtype=release -Dpipelines=auto -Dipas=rpi/vc4 -Dv4l2=true -Dgstreamer=enabled -Dtest=false -Dlc-compliance=disabled -Dcam=disabled -Dqcam=enabled -Ddocumentation=disabled
ninja -C build   # use -j 2 on Pi 3 or earlier devices
ninja -C build install

#Installing server dependencies
echo ">> Installing Server dependencies"
apt-get install libpq-dev

python3 -m pip install psycopg2
python3 -m pip install matplotlib
python3 -m pip install apscheduler

apt-get install libxcb-xinerama0

#installing Boost dependencies
echo ">> Installing boost dependencies"
apt install libboost-all-dev

#Installing python dependencies
echo ">> Installing dependencies for program..."

# Update and upgrade
apt update && apt upgrade -y

# Get coral set up - see https://coral.ai/docs/accelerator/get-started/
echo ">> Installing pycoral..."
echo "deb https://packages.cloud.google.com/apt coral-edgetpu-stable main" | sudo tee /etc/apt/sources.list.d/coral-edgetpu.list
curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -
apt update
apt-get install -y libedgetpu1-std
apt-get install -y python3-pycoral

# Install dependencies for program
echo ">> Installing python dependencies..."
apt install -y python3-dev python3-pip 
python3 -m pip install opencv-python==4.5.5.64
python3 -m pip install opencv-contrib-python==4.5.5.64 

# More dependencies
apt-get -y install libgl1-mesa-glx

echo ">> Installing gdown..."
apt-get install python3-pip
pip3 install gdown

echo ">> Downloading additional files for program..."
cd $DIR

gdown --folder https://drive.google.com/drive/folders/1xp-MMvOHN0dpLNeSs2ASOwO7C8mZ74f9

cd fishCensDependencies

mv models vid $DIR/py
mv sql ~
mv systemctl/* /etc/systemd/system/

cd $DIR
rm -rf fishCensDependencies

# Make build folder and build program
echo ">> Building program..."
cd $DIR
mkdir build
mkdir build/vid
mkdir build/models
cp -R $DIR/py/vid/* $DIR/build/vid
cp -R $DIR/py/models/* $DIR/build/models

chmod +x $DIR/*.sh

# Get NGINX and SQL Server downloaded
echo ">> Downloading NGINX and other server dependencies"
# Install NGINX
apt-get update
apt-get install -y nginx

# Install PostgreSQL
apt-get install -y postgresql postgresql-contrib

# Install Python 3 and pip
apt-get install -y python3 python3-pip

# Install ap scheduler for background tasks
pip3 install apscheduler
pip3 install matplotlib

#Install pqxx
apt install libpqxx-dev

# Install Flask and required packages
pip3 install flask
pip3 install flask_sqlalchemy
pip3 install psycopg2-binary
pip3 install gunicorn

# Setup NGINX configuration file
ln -s /etc/nginx/sites-available/default /etc/nginx/sites-enabled/

# Restart NGINX
systemctl restart nginx

# Downloading server and building graph for first time
echo ">> Downloading FC Server and then running graph just to get a file in the folder"
cd ~
git clone https://github.com/jbates35/fcServer
cd ~/fcServer
python3 sensorGrapher.py

