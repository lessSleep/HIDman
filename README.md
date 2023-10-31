# HIDman
## USB HID to PS/2 converter

This is a project to develop a device to adapt USB HID devices (keyboards, mice, joysticks) to work on PCs that support PS/2 devices.

Currently only PS/2 computers are supported but plans are underway to support computers that require serial mice and XT keyboards.

As a bonus, it allows you to connect a USB game controller and have it emulate a PS/2 keyboard and mouse - this allows you to use a gamepad to play games that never had joystick support.

## Technical description

The HIDman is based around the CH559 from WCH, a remarkably flexible chip with **two** USB HOST ports. This makes it ideal for our purposes.

The code is forked from atc1441's excellent repository - https://github.com/atc1441/CH559sdccUSBHost

PCB and enclosure was designed in KiCad - source files are in the hardware directory.

Development is very active but it is usable in its current state.



# 创建docker环境
启动docker服务
sudo service docker start

下面两条命令功能一样，使用一个就行
docker run --name sdcc -it --privileged -v /home/zhangling/gtr:/root/gtr -w /root/gtr ubuntu:22.04

docker run --name sdcc -it --privileged -v /home/ubuntu2004/gtr/hidman-mini:/root/hidman-mini -w /root/hidman-mini ubuntu:22.04

上面的已经启动docker了


docker start sdcc
docker exec -it sdcc bash

# 进入docker环境，安装开发工具
安装系统自带的开发工具
apt update
apt install -y build-essential

下载sdcc开发工具
sdcc-4.3.0-amd64-unknown-linux2.5.tar.bz2
# 编译工程

git clone https://github.com/atc1441/CH559sdccUSBHost 
cp ../CH559sdccUSBHost/CH559.h .  这个不需要
cd 
mkdir build

export PATH=$PATH:/root/hidman-mini/sdcc-4.3.0/bin

在当前目录make，不要去build
make

# 参考

apt install -y cmake  # 这个用不到

