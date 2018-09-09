#force swapping to software ECC via control groups

sudo mkswap /dev/hamming0
sudo swapon --priority 32767 /dev/hamming0
sysctl vm.swappiness=100

#can I simply unmount it like this?

sudo umount /cgroup
sudo mount -t cgroup -o memory cgroup /cgroup

# memory restrictions can't be put on the root cgroup, so we have to
# put everything in a child

mkdir /cgroup/child

# 8M seems like enough for all processes

# If we need more or less speed, this is the variable to change, since
# the overhead from this system is contant swapping, and 8M was chosen as
# a compromise between protection and security

echo "8M" | sudo tee /cgroup/child/memory.limit_in_bytes

#disable OOM killer, AFAIK this only kills processes when memory is not readily
#available, but since we have plenty of cache, we should be able to swap out easily,
#right?
echo "1" | sudo tee /cgroup/child/memory.oom_control
#swappiness is inherited from sysctl

#update.sh will be a cron job to put all new processes into the memory limited cgroup
/update.sh
