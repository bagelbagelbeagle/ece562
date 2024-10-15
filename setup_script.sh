#!/bin/bash

# Function to check if the root password is set
check_root_password() {
    if sudo -n true 2>/dev/null; then
        echo "Root password is set."
    else
        echo "Root password is not set or sudo privileges are required."
        set_root_password
    fi
}

# Function to set the root password
set_root_password() {
    echo "You need to set a root password to proceed."
    echo "Please enter a new root password:"
    sudo passwd root
}

# Check if the script is being run with root privileges
#if [ "$EUID" -ne 0 ]; then
#    echo "This script must be run as root. Please run with sudo."
#    exit 1
#fi

# Check if root password is set
check_root_password

# If the root password is set, continue with the script

# Install required packages
sudo apt-get update
sudo apt-get install -y curl zip unzip tar g++ make

# Clone ChampSim
git clone https://github.com/ChampSim/ChampSim.git

# Vcpkg commands
cd ./ChampSim
git submodule update --init
sudo apt install pkg-config
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install


# Download and Install Pin
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
tar zxf pin-3.22-98547-g7a303a835-gcc-linux.tar.gz

# Build Pin
cd pin-3.22-98547-g7a303a835-gcc-linux/source/tools
make

# Set PIN_ROOT environment variable (replace /your/path/to/pin with actual path)
echo "export PIN_ROOT=~/ChampSim/pin-3.22-98547-g7a303a835-gcc-linux/" >> ~/.bashrc

# Clone the ece562 repository
#cd ~
#git clone https://github.com/bagelbagelbeagle/ece562.git

echo "Cleaning up..."
cd ~/ChampSim
rm -r ./pin-3.22-98547-g7a303a835-gcc-linux.tar.gz

echo "Script execution completed successfully."
