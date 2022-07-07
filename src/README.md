readme:
==
说明：
--
```
这份代码是从qsdk中分离出来的，对Makefile文件做了部分修改
```
预置条件
--
```
选择一个编译好的openwrt树，将代码clone到package目录下，执行make menuconfig进行配置。
LEDE代码树不行，编译的时候报错，没有细看
```
编译配置(ar750s)
--
```
Target System (Atheros AR7xxx/AR9xxx)  --->
Boot Loaders  --->
	<*> GL-uboot-ar750s.......................... U-boot for AR750S
```
编译配置(ar750)
--
```
Target System (Atheros AR7xxx/AR9xxx)  --->
Boot Loaders  --->
	<*> GL-uboot-ar750.......................... U-boot for AR750
```
编译配置(x750-4g)
--
```
Target System (Atheros AR7xxx/AR9xxx)  --->
Boot Loaders  --->
	<*> GL-uboot-x750-4g.......................... U-boot for x750-4g
```
