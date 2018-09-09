# My HDD for my testing laptop is an 8GB flash drive, so actual IO
# operations lag, and I don't want to mistake this as a bug in the
# driver itself

echo "RAW (/dev/hamming0)"
dd if=/dev/zero of=/dev/hamming0
dd if=/dev/hamming0 of=/dev/null

# TODO: did the kernel I sent out with the VM have /dev/ram0 enabled?
# may have accidentially left that disabled...
#echo "RAW (/dev/ram0)"
#dd if=/dev/zero of=/dev/ram0
#dd if=/dev/ram0 of=/dev/null


echo "Write to disk (any latencies caused here are purely disk)"

touch TEST_BLOCK
rm -rf TEST_BLOCK
dd if=/dev/urandom of=TEST_BLOCK count=8192

dd if=TEST_BLOCK of=/dev/hamming0 count=8192
rm -rf TEST_BLOCK_2
dd if=/dev/hamming0 of=TEST_BLOCK_2 count=8192
