This repo is uboot source code which modified by GL.iNet team.
The source code in the repository is compiled under the environment of OpenWrt-18.06.5

### Prerequisites
It depends on OpenWRT toolchain. We assume that you have compiled firmware from buildroot. Downloading this repo to your OpenWRT buildroot source tree.

```
$ cd openwrt-buildroot/package  
$ git clone https://github.com/gl-inet/uboot-for-qca95xx.git
$ cd ../
```
### Compile
**Sealect AR71XX NAND target**

```
Target System (Atheros AR7xxx/AR9xxx)  ---> 
Subtarget (Generic devices with NAND flash)  --->
Target Profile (GL-AR750S with nand flash)  --->
```
**Select the UBoot type you want to compile** (Note that if you need to compile uboot for the E750, please select mifi-v3)

```
Boot Loaders  ---> 
	<M> GL-uboot-ar300m........................................ U-boot for ar300m
	< > GL-uboot-ar750s........................................ U-boot for ar750s
	...
```
```
$ make ./package/gl-95xx-uboot/compile
```
After the compilation is complete, look under openWrt-buildroot/bin/targets/ar71xx/generic/ for the output binary.

