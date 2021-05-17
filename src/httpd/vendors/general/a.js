

var i=1;
var innerDom = document.getElementsByClassName('innerText');
var text0 = "The firmware version must be 3.201 or newer";
var text1 = "The firmware version must be 3.201 or newer. Any older or third party firmware may brick this device!";
var text2 = "uboot2.0 version:19.11.12";
if (i==1) {
	
	innerDom[0].innerHTML = text0;
	innerDom[1].innerHTML = text1;
	innerDom[2].innerHTML = text2;
}

else innerDom.remove()

