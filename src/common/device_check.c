#include <common.h>
#include <watchdog.h>
#include <command.h>
#ifdef CONFIG_MODEM_SUPPORT
#include <malloc.h>		/* for free() prototype */
#endif

#ifdef CFG_HUSH_PARSER
#include <hush.h>
#endif

#include <post.h>

/* for isdigit() */
#include <linux/ctype.h>
#include <asm-mips/string.h>

#define  __DEBUG__
#ifdef __DEBUG__
#define DEBUG(formt,...) printf(formt,##__VA_ARGS__)
#else
#define DEBUG(formt,...)
#endif

extern char nand_boot_failed;
extern char tftp_file;
extern int do_nand(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[]);
int check_nand()
{
	int argc=2;
	char *argv[2];
	argv[0] = "2";
	argv[1] = "bad";
	return !do_nand(NULL,0,argc,argv);
}

extern int do_ping (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
int check_tftp_file()
{
   	int argc=2;
	char *argv[2];
	argv[0] = "2";
	argv[1] = "192.168.1.2";

	if(do_ping(NULL,0,argc,argv))return 0;
	tftp_file = 1;
	run_command("tftp 80010000 openwrt-gl-ar300m.bin",0);
	if(tftp_file){
		setenv("tmp_env","erase 0x9f050000 +$filesize && cp.b $fileaddr 0x9f050000 $filesize");
		run_command("run tmp_env",0);
	}
	
	if(0==check_nand()) return 0;

	tftp_file = 1;
	run_command("tftp 80010000 openwrt-gl-ar300m-ubi.img",0);
	if(tftp_file){
		setenv("tmp_env"," nand erase && nand write $fileaddr 0 $filesize");
		run_command("run tmp_env",0);
	}
	return 0;
}


int select_boot_dev(){
	char *dev=NULL;
	unsigned int val=0;
		
	dev = getenv("boot_dev");
	if(strcmp(dev,"on") == 0)
	{
		val = switch_boot_load();
		//printf("val is %d\n",val);
		switch(val)//from nand boot
		{
		case 2: run_command("nboot 0x81000000 0",0);break;
		case 1: run_command("bootm 0x9f050000",0);break;
		default: break;
		}		
	}
}
/****************************************************************************/
/*
 * Check if test mode is finished
 * @return: 1: done 0: not done
 */
int test_done(void){
	volatile unsigned int *s=(volatile unsigned int *)0x9fff0060;
	if(*s==0x7365636f) //seco
		return 1;
	return 0;
}
/*
 * Check the calibration status
 * -1: no art found, stop
 * 0 : not calibrated, booting calibration firmware
 * 1 : calibrated, and but need to write config
 * 2 : calibrated, and config ready
 */
int calibration_status(void){

	int ret=-1;
	int calibrated=0;
	int has_art_final=0;
	int has_config=0;
	int has_test=0;
	int has_nand=0;
	//volatile unsigned long *art_cal=(volatile unsigned long *)0x9f3f1000;
	volatile unsigned short *art_final=(volatile unsigned short *)0x9fff1000;
	volatile unsigned int *config_data=(volatile unsigned int *)0x9fff0010;
//	volatile unsigned long *abeg=(volatile unsigned long *)0x9fff1138;
	volatile unsigned char *v2=(volatile unsigned char *)0x9fff108f;
	volatile unsigned char *v3=(volatile unsigned char *)0x9fff1095;
	volatile unsigned char *v4=(volatile unsigned char *)0x9fff109b;

//	int has_art_cal=0;
//	if((*abeg & 0x0000ffff)==0x4142 && *(abeg+1)==0x45473132){
//		has_art_cal=1;
//	}

	has_test = test_done();
	has_nand = check_nand();
	if(*config_data!=0xffffffff){
		has_config=1;
	}
	//already calibrated
	if(*v2!=0 && *v3!=0 && *v4!=0){
		calibrated=1;
	}

	if(*art_final==0x0202){
		has_art_final=1;
	}

	if(has_art_final){
		DEBUG("Found ART,Checking calibration status...\n");
		if(calibrated){
			DEBUG("Device have calibrated,Checking device test status...\n");
			//DEBUG("skip device test status,Checking MAC address...\n");
			if(has_test){
				DEBUG("Device have tested,Checking MAC address...\n");
					if(has_config){
						DEBUG("Device have MAC address,Checking device flash status...\n");
						if(has_nand){
							if(nand_boot_failed){
								DEBUG("Device have nor and nand flash,Booting standard firmware from nor flash...\n");
								return 0;
								}
							else{
								DEBUG("Device have nor and nand flash,Booting standard firmware from nand flash...\n");
								return 1;
								} 
							}
						else{
							DEBUG("Device only have nor flash,Booting standard firmware from nor flash...\n");
							return 2;
							}											
					}else{
					DEBUG("Device haven't MAC address,please write MAC in calibration firmware...\n");;
					return 3;
					}
				    }
			else{
				DEBUG("Device haven't tested. Please test device in calibration firmware...\n");
				return 4;			
				}
		}else{
			DEBUG("Device not calibrated. Booting the calibration firmware...\n");
			return 5;
		}
	}else{
		DEBUG("Device don't have ART,please write the default ART first...\n");
		return 6;
	}

}


