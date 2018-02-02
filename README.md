update: 20180202

这个是适用于生产4.0的uboot源码

编译方法：

1. 下载我们自己的SDK，openwrt或LEDE都可以
2. 把整个代码clone在package目录下
3. make menuconfig
	Subtarget (Generic devices with NAND flash)  ---> 
	Target Profile (GL-AR300M NAND)  --->
5. make package/uboot-9531/compile V=s
6. 编译好的二进制在bin/ar71xx/openwrt-ar71xx-cus531-nand-qca-legacy-uboot.bin
