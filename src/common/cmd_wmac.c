#include <common.h>
#include <command.h>
#include "defines.h"

static int mac_add(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int rc,i;
	u_char  linebuf[6];
	u_char  *ucp = linebuf;
	ulong saddr = simple_strtoul(argv[1], NULL, 16);
	ulong daddr = simple_strtoul(argv[2], NULL, 16);
	uchar snum = simple_strtoul(argv[3], NULL, 16);
	
	if(argc != 4){
		printf("Usage:\n	- wmac saddr daddr shiftnum\n");
		return -1;
	}

	for(i=0; i<6; i++)
			*((u_char *)ucp++) = *((u_char *)saddr++);

	linebuf[5] += snum;
	for(i=0; i<6; i++)
		*((u_char *)daddr++) = linebuf[i];
	
	return 0;
}

U_BOOT_CMD(
	wmac,4,1,mac_add,
	"wmac	-  source_addr destionation_addr shift_num\n",
	" -wmac 0x80100000 0x801100000 1\n "
);
