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
#include <nand.h>

/* for isdigit() */
#include <linux/ctype.h>
#include <asm-mips/string.h>
#include <config.h>

#define  __DEBUG__
#ifdef __DEBUG__
#define DEBUG(formt,...) printf(formt,##__VA_ARGS__)
#else
#define DEBUG(formt,...)
#endif

extern char nand_boot_failed;
extern char tftp_file;
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
extern int do_nand(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[]);
int check_nand()
{
	int argc=2;
	char *argv[2];
	argv[0] = "2";
	argv[1] = "bad";
	return !do_nand(NULL,0,argc,argv);
}

int check_nand_data()
{
	u_char buf[20]={0};
	ulong addr=0, off=0, size=20;
	int i=0;
	
	nand_info_t *nand=NULL;
	nand = &nand_info[0];
	nand_read(nand,off,&size,buf);
	for(i=0;i<20;i++){
		if(buf[i] != 0XFF)
			return 1;
	}
	
	return 0;
}
#endif

int check_tftp_file()
{
#ifdef CONFIG_XE300
	return 0;
#endif

#ifndef CONFIG_GL_RSA
	run_command("run lf",0);
#endif
	return 0;
}


int select_boot_dev(){
#ifdef CONFIG_AR300M
	char *dev=NULL;
	char *boot_local=NULL;
	unsigned int val=0;
	char boot_cmd[30]={0};
		
	dev = getenv("boot_dev");
	boot_local = getenv("boot_local");
	if(dev != NULL){
		if(strcmp(dev,"on") == 0)
		{

			val = switch_boot_load();
			printf("val is %d\n",val);
			switch(val)//from nand boot
			{
			case 0:
				sprintf(boot_cmd,"bootm 0x%x",GL_BOOT_ADDR);
				run_command(boot_cmd,0);
				break;
			case 1:
			case 2:
				run_command("nboot 0x81000000 0",0);
				break;
			default:
				break;
			}		
		}
	}
	if(boot_local != NULL){
		if(strcmp(boot_local,"nor") == 0){
			run_command("bootm 0x9f050000",0);
			}
		else{
			run_command("nboot 0x81000000 0",0);
			}
	}
#endif
	return 0;


}
/****************************************************************************/
/*
 * Check if test mode is finished
 * @return: 1: done 0: not done
 */
int test_done(void){
	volatile unsigned int *s=(volatile unsigned int *)(GL_ART_ADDR + 0X60);
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
	int nand_data=0;
	//volatile unsigned long *art_cal=(volatile unsigned long *)0x9f3f1000;
	volatile unsigned short *art_final=(volatile unsigned short *)(GL_ART_ADDR + 0X1000);
	volatile unsigned int *config_data=(volatile unsigned int *)(GL_ART_ADDR + 0X10);
//	volatile unsigned long *abeg=(volatile unsigned long *)0x9fff1138;
	volatile unsigned char *v2=(volatile unsigned char *)(GL_ART_ADDR + 0X108f);
	volatile unsigned char *v3=(volatile unsigned char *)(GL_ART_ADDR + 0X1095);
	volatile unsigned char *v4=(volatile unsigned char *)(GL_ART_ADDR + 0X109b);

//	int has_art_cal=0;
//	if((*abeg & 0x0000ffff)==0x4142 && *(abeg+1)==0x45473132){
//		has_art_cal=1;
//	}

	has_test = test_done();
#if (CONFIG_COMMANDS & CFG_CMD_NAND)
	has_nand = check_nand();
#endif
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
#ifndef CONFIG_AR300M //only 300m check bootcount
	nand_boot_failed = 1;
#endif
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
									if( check_nand_data() ){
										DEBUG("Device have nor and nand flash,Booting standard firmware from nand flash...\n");
										return 1;
									}
									else{
										DEBUG("Device have nor and nand flash,Nand no data,Booting standard firmware from nor flash...\n");
										return 0;
									}
								} 
							}
						else{
							DEBUG("Device only have nor flash,Booting standard firmware from nor flash...\n");
							nand_boot_failed = 1;
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


