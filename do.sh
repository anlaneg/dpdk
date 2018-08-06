#! /bin/bash
function ubuntu_install()
{
	echo "ubuntu install"
	sudo apt install -y gcc libnuma-devel
}

function centos_install()
{
	echo "centos install"
	sudo yum install -y gcc numactl-devel 
}

function UNKOWN_install()
{
	echo "os unkown,quit"
	exit 1
}

function detect_os()
{
	name=`cat /etc/os-release | grep "\<NAME\>" | cut -d':' -f 2 | tr [:upper:] [:lower:]`
	if  `echo "$name" | grep -q "centos"`  ;
	then
		echo "centos"

	elif `echo "$name" | grep -q "ubuntu"` ;
	then
		echo "ubuntu"

	else
		echo "UNKOWN"
	fi;
}

function install()
{
	os=`detect_os`
	${os}_install
	
}

install
export RTE_TARGET=x86_64-native-linuxapp-gcc
export EXTRA_CFLAGS="-g -O0"
make config T=$RTE_TARGET
make V=1 -j20
#make install -j20 T=$RTE_TARGET
make V=1 install -j20 DESTDIR=$RTE_TARGET

