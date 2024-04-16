# Install git
sudo add-apt-repository -y ppa:git-core/ppa
sudo apt install -y git
git --version

# Install CMake
sudo wget -qO /etc/apt/trusted.gpg.d/kitware-key.asc https://apt.kitware.com/keys/kitware-archive-latest.asc
echo "deb https://apt.kitware.com/ubuntu/ $(lsb_release -sc) main" | sudo tee /etc/apt/sources.list.d/kitware.list
sudo apt update
sudo apt install -y cmake
cmake --version

# Install gcc-arm
#ARM_TOOLCHAIN_VERSION=$(curl -s https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads | grep -Po '<h4>Version \K.+(?=</h4>)')
#curl -Lo gcc-arm-none-eabi.tar.xz "https://developer.arm.com/-/media/Files/downloads/gnu/10.3/binrel/arm-gnu-toolchain-10.3-x86_64-arm-none-eabi.tar.xz"

curl -Lo gcc-arm-none-eabi.tar.bz2 "https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2"
sudo mkdir /opt/gcc-arm-none-eabi
#sudo tar xf gcc-arm-none-eabi.tar.xz --strip-components=1 -C /opt/gcc-arm-none-eabi
sudo tar xf gcc-arm-none-eabi.tar.bz2 --strip-components=1 -C /opt/gcc-arm-none-eabi

echo 'export PATH=$PATH:/opt/gcc-arm-none-eabi/bin' | sudo tee -a /etc/profile.d/gcc-arm-none-eabi.sh
source /etc/profile
arm-none-eabi-gcc --version
arm-none-eabi-g++ --version
rm -rf gcc-arm-none-eabi.tar.xzs

# Install pico sdk
sudo git clone https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk
sudo git -C /opt/pico-sdk submodule update --init
echo 'export PICO_SDK_PATH=/opt/pico-sdk' | sudo tee -a /etc/profile.d/pico-sdk.sh
source /etc/profile.d/pico-sdk.sh

# Install BP5
sudo git clone https://github.com/DangerousPrototypes/BusPirate5-firmware.git bp5-main
cd bp5-main
md build
cs build 
cmake ..
make 
cd ~
md webhook
pip install virtualenv
apt install python3.10-venv
python3.10 -m venv env
sudo apt install build-essential libssl-dev libffi-dev python3-dev
source env/bin/activate
pip3 install flask 
pip3 install github_webhook
pip3 install requests
sudo iptables -A INPUT -p tcp --dport 80 -j ACCEPT
sudo apt-get install iptables-persistent
screen
python3 webhook.py  




