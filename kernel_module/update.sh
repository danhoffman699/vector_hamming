if [ -e "/cgroup/cgroup.procs" ]
then
	for pid in `ps -ef | awk '{print $2}'`
	do
		echo $pid | sudo tee /cgroup/child/cgroup.procs
	done
fi
