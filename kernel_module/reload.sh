sudo rmmod hamming
make
sudo insmod hamming.ko
sleep 1
dmesg
