update: 20170228
fix makefile, now can be compiled in any openwrt tree

这个是基于赵刚的uboot修改的，要修正几个问题：

1. 首先要适应卓翼的生产流程，流程在下面说明
2. 加入几个功能：可以用switch选择启动哪个固件，可以web刷两个不同的固件，可以定制switch功能
3. 解决MTU问题，在某些机器，比如windows上面，web刷机不行

编译方法：

1. 找一个编译好的openwrt代码树
2. 把整个代码clone在package/qca-legacy-uboot-nand 的目录里面。如果用qsdk，要注意他的代码在boot/qca-legacy-uboot-nand 下面，把那个文件夹的内容全部删掉或者替换
3. make menuconfig
4. 选择qca-legacy-uboot-cus531-nand
5. make package/qca-legacy-uboot-nand/compile V=s
6. 编译好的二进制在bin/ar71xx/openwrt-ar71xx-cus531-nand-qca-legacy-uboot.bin
7.
