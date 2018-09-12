# U-boot for AR300M  

This repo is uboot source code which modified by GL.iNet team.  

# Prerequisites  

It depends on OpenWRT toolchain. We assume that you have compiled firmware from buildroot. Downloading this repo to your OpenWRT buildroot source tree.  

```bash  
$ cd your/buildroot/path/package  
$ git clone https://github.com/gl-inet/gl-uboot-source-for--ar300m uboot-9531
$ cd -
```  

# Compiling U-boot  

As you know, u-boot is a single package on buildroot, now we can compile it easily. Note that you need to select *Subtarget (Generic devices with NAND flash) --->* and *Target Profile (GL-AR300M NAND) --->* via `make menuconfig` at first.

```  
make package/uboot-9531/compile V=s
```  

The final binary file is located in **bin/ar71xx/openwrt-ar71xx-cus531-nand-qca-legacy-uboot.bin**.  
