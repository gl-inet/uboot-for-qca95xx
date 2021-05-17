
var j=1;
var innerDom_uboot = document.getElementsByClassName('ubootText');
var uboottext0 = "The uboot version must be 20.12.22 or newer";
var uboottext1 = "The uboot version must be 20.12.22 or newer. Any older or third party uboot may brick this device!";
var uboottext2 = "uboot2.0 version:20.12.22";
if (j==1) {
	
	innerDom_uboot[0].innerHTML = uboottext0;
	innerDom_uboot[1].innerHTML = uboottext1;
	innerDom_uboot[2].innerHTML = uboottext2;
}

else innerDom_uboot.remove()
