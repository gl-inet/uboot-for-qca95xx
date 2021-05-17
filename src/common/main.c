/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* #define	DEBUG	*/

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
#include <config.h>

#ifdef CONFIG_SILENT_CONSOLE
DECLARE_GLOBAL_DATA_PTR;
#endif

#ifdef CONFIG_DUALIMAGE_SUPPORT
extern unsigned findbdr(unsigned int flashaddr);
#endif

#if defined(CONFIG_BOOT_RETRY_TIME) && defined(CONFIG_RESET_TO_RETRY)
extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);		/* for do_reset() prototype */
#endif

extern int do_bootd (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);


#define MAX_DELAY_STOP_STR 32

static char * delete_char (char *buffer, char *p, int *colp, int *np, int plen);
static int parse_line (char *, char *[]);
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
static int abortboot(int);
#endif

#undef DEBUG_PARSER

char        console_buffer[CFG_CBSIZE];		/* console I/O buffer	*/

static char erase_seq[] = "\b \b";		/* erase sequence	*/
static char   tab_seq[] = "        ";		/* used to expand TABs	*/

#ifdef CONFIG_BOOT_RETRY_TIME
static uint64_t endtime = 0;  /* must be set, default is instant timeout */
static int      retry_time = -1; /* -1 so can call readline before main_loop */
#endif

#define	endtick(seconds) (get_ticks() + (uint64_t)(seconds) * get_tbclk())

#ifndef CONFIG_BOOT_RETRY_MIN
#define CONFIG_BOOT_RETRY_MIN CONFIG_BOOT_RETRY_TIME
#endif

#ifdef CONFIG_MODEM_SUPPORT
int do_mdm_init = 0;
extern void mdm_init(void); /* defined in board.c */
#endif
char nand_boot_failed = 0;
char tftp_file = 1;
/***************************************************************************
 * Watch for 'delay' seconds for autoboot stop or autoboot delay string.
 * returns: 0 -  no key string, allow autoboot
 *          1 - got key string, abort
 */
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
# if defined(CONFIG_AUTOBOOT_KEYED)
static __inline__ int abortboot(int bootdelay)
{
	int abort = 0;
	uint64_t etime = endtick(bootdelay);
	struct
	{
		char* str;
		u_int len;
		int retry;
	}
	delaykey [] =
	{
		{ str: getenv ("bootdelaykey"),  retry: 1 },
		{ str: getenv ("bootdelaykey2"), retry: 1 },
		{ str: getenv ("bootstopkey"),   retry: 0 },
		{ str: getenv ("bootstopkey2"),  retry: 0 },
	};

	char presskey [MAX_DELAY_STOP_STR];
	u_int presskey_len = 0;
	u_int presskey_max = 0;
	u_int i;

#ifdef CONFIG_SILENT_CONSOLE
	if (gd->flags & GD_FLG_SILENT) {
		/* Restore serial console */
		console_assign (stdout, "serial");
		console_assign (stderr, "serial");
	}
#endif

#  ifdef CONFIG_AUTOBOOT_PROMPT
	printf (CONFIG_AUTOBOOT_PROMPT, bootdelay);
#  endif

#  ifdef CONFIG_AUTOBOOT_DELAY_STR
	if (delaykey[0].str == NULL)
		delaykey[0].str = CONFIG_AUTOBOOT_DELAY_STR;
#  endif
#  ifdef CONFIG_AUTOBOOT_DELAY_STR2
	if (delaykey[1].str == NULL)
		delaykey[1].str = CONFIG_AUTOBOOT_DELAY_STR2;
#  endif
#  ifdef CONFIG_AUTOBOOT_STOP_STR
	if (delaykey[2].str == NULL)
		delaykey[2].str = CONFIG_AUTOBOOT_STOP_STR;
#  endif
#  ifdef CONFIG_AUTOBOOT_STOP_STR2
	if (delaykey[3].str == NULL)
		delaykey[3].str = CONFIG_AUTOBOOT_STOP_STR2;
#  endif

	for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i ++) {
		delaykey[i].len = delaykey[i].str == NULL ?
				    0 : strlen (delaykey[i].str);
		delaykey[i].len = delaykey[i].len > MAX_DELAY_STOP_STR ?
				    MAX_DELAY_STOP_STR : delaykey[i].len;

		presskey_max = presskey_max > delaykey[i].len ?
				    presskey_max : delaykey[i].len;

#  if DEBUG_BOOTKEYS
		printf("%s key:<%s>\n",
		       delaykey[i].retry ? "delay" : "stop",
		       delaykey[i].str ? delaykey[i].str : "NULL");
#  endif
	}

	/* In order to keep up with incoming data, check timeout only
	 * when catch up.
	 */
	while (!abort && get_ticks() <= etime) {
		for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i ++) {
			if (delaykey[i].len > 0 &&
			    presskey_len >= delaykey[i].len &&
			    memcmp (presskey + presskey_len - delaykey[i].len,
				    delaykey[i].str,
				    delaykey[i].len) == 0) {
#  if DEBUG_BOOTKEYS
				printf("got %skey\n",
				       delaykey[i].retry ? "delay" : "stop");
#  endif

#  ifdef CONFIG_BOOT_RETRY_TIME
				/* don't retry auto boot */
				if (! delaykey[i].retry)
					retry_time = -1;
#  endif
				abort = 1;
			}
		}

		if (tstc()) {
			if (presskey_len < presskey_max) {
				presskey [presskey_len ++] = getc();
			}
			else {
				for (i = 0; i < presskey_max - 1; i ++)
					presskey [i] = presskey [i + 1];

				presskey [i] = getc();
			}
		}
	}
#  if DEBUG_BOOTKEYS
	if (!abort)
		puts ("key timeout\n");
#  endif

#ifdef CONFIG_SILENT_CONSOLE
	if (abort) {
		/* permanently enable normal console output */
		gd->flags &= ~(GD_FLG_SILENT);
	} else if (gd->flags & GD_FLG_SILENT) {
		/* Restore silent console */
		console_assign (stdout, "nulldev");
		console_assign (stderr, "nulldev");
	}
#endif

	return abort;
}

# else	/* !defined(CONFIG_AUTOBOOT_KEYED) */

#ifdef CONFIG_MENUKEY
static int menukey = 0;
#endif
static ulong my_atoi(const char *c);
extern char firmware_name[16];
static int hex_str_to_num(char *str, int length)
{
    char revstr[16] = {0}; //根据十六进制字符串的长度，这里注意数组不要越界
    int num[16] = {0};
    int count = 1;
    int result = 0;
           
    strcpy(revstr, str);

    for(int i = length - 1; i >= 0; i--)
    {
        if((revstr[i] >= '0') && (revstr[i] <= '9')){
            num[i] = revstr[i] - 48;   //字符0的ASCII值为48                  
        }
        else if((revstr[i] >= 'a') && (revstr[i] <= 'f')){
            num[i] = revstr[i] - 'a' + 10;
        }
        else if((revstr[i] >= 'A') && (revstr[i] <= 'F')){
            num[i] = revstr[i] - 'A' + 10;
        }
        else{
            num[i] = 0;                 
        }

        result += num[i] * count;
        count *= 16; //十六进制(如果是八进制就在这里乘以8)           
    }

    return result;
}

extern uint show_kernel(uint addr);
extern char gl_set_uip_info();
extern void gl_upgrade_probe();
extern void gl_upgrade_listen();
extern void exception_led_indicator();

extern char gl_probe_upgrade;
static __inline__ int abortboot(int bootdelay)
{
	int abort = 0;
    char tftp_command[50] = "0";
    char cp_command[50] = "0";
    char nandw_command[50] = "0";
    char erase_command[50] = "0";
#ifdef CONFIG_SILENT_CONSOLE
	if (gd->flags & GD_FLG_SILENT) {
		/* Restore serial console */
		console_assign (stdout, "serial");
		console_assign (stderr, "serial");
	}
#endif
#ifdef CONFIG_MENUPROMPT
	printf(CONFIG_MENUPROMPT, bootdelay);
#else

   // run_command("dhcpd start", 0);
	printf("Hit 'gl' to stop autoboot: %2d ", bootdelay);
#endif

#if defined CONFIG_ZERO_BOOTDELAY_CHECK
	/*
	 * Check if key already pressed
	 * Don't check if bootdelay < 0
	 */
	if (bootdelay >= 0) {
		if (tstc()) {	/* we got a key press	*/
			(void) getc();  /* consume input	*/
			puts ("\b\b\b 0");
			abort = 1; 	/* don't auto boot	*/
		}
	}
#endif
	char tmp_flag=0,tmp_key=0;

	while ((bootdelay >= 0) && (!abort)) {
		int i;
		gl_probe_upgrade = gl_set_uip_info();

		/* delay 100 * 10ms */
		for (i=0; !abort && i<100; ++i) {
			if(gl_probe_upgrade){
				gl_upgrade_probe();
				gl_upgrade_listen();
			}
				
#if 0
		
            if(update_msg()){
                abort  = 1; 
                bootdelay = 1;
                printf("V 1.0.0\n");
                sprintf(tftp_command,"tftpboot 0x81000000 %s",firmware_name);
                printf("%s\n",tftp_command);
                run_command(tftp_command, 0);
                printf("filesize:%d \n",hex_str_to_num(getenv("filesize"),strlen(getenv("filesize"))));
                if(hex_str_to_num(getenv("filesize"),strlen(getenv("filesize")))<=0){
                    printf("file err\n");
                    break;
                }
                if(strstr(firmware_name,".bin")){
                    if(327680<hex_str_to_num(getenv("filesize"),strlen(getenv("filesize")))){ 

                        sprintf(erase_command,"erase %s  $filesize",getenv("firmware_addr");
                        sprintf(cp_command,"cp.b 0x81000000  %s $filesize",getenv("firmware_addr");
                        printf("%s\n",erase_command);
                        printf("%s\n",cp_command);
                    }
                    else{
                        sprintf(erase_command,"erase %s +%s",getenv("uboot_addr"),getenv("uboot_size"));
                        sprintf(cp_command,"cp.b 0x81000000 %s %s",getenv("uboot_addr"),getenv("filesize"));
                        printf("%s\n",erase_command);
                        printf("%s\n",cp_command);
                    }
                    run_command(erase_command, 0);
                    run_command(cp_command, 0);
                }
                else if(strstr(firmware_name,".img")){
                    sprintf(erase_command,"erase %s +0x%x",getenv("firmware_addr"),show_kernel(0x81000000));
                    sprintf(cp_command,"cp.b 0x81000000  %s 0x%x",getenv("firmware_addr"),show_kernel(0x81000000));
                    printf("%s\n",erase_command);
                    printf("%s\n",cp_command);
                    run_command(erase_command, 0);
                    run_command(cp_command, 0);
                    run_command("nand erase && nand write 0x81200000 0 0xae0000" , 0);
                }
                else if(strstr(firmware_name,".ubi")){
                    sprintf(erase_command,"erase %s +0x%x",getenv("firmware_addr"),show_kernel(0x81000000));
                    sprintf(cp_command,"cp.b 0x81000000  %s 0x%x",getenv("firmware_addr"),show_kernel(0x81000000));
                    printf("%s\n",erase_command);
                    printf("%s\n",cp_command);
                    run_command(erase_command, 0);
                    run_command(cp_command, 0);
                }
                printf("Update Done");
                send_U_G_ok(); 
                run_command("res", 0);
                break;
            }
#endif
			if (tstc()) {	/* we got a key press	*/
				//abort  = 1;	/* don't auto boot	*/
				//bootdelay = 0;	/* no more delay	*/
# ifdef CONFIG_MENUKEY
				menukey = getc();
# else
				//(void) getc();  /* consume input	*/
                                tmp_key = getc();
                                if(tmp_key == 'g'){
                                        tmp_flag = 1;
                                        break;
                                }
                                if((tmp_flag == 1)&&(tmp_key == 'l'))
                                {
                                        abort  = 1;     /* don't auto boot      */
                                        bootdelay = 1;  /* no more delay        */
										gl_probe_upgrade = 0;
                                        eth_halt();
                                        break;
                                }
# endif
				break;
			}
			udelay (10000);
		}
		--bootdelay;

		printf ("\b\b\b%2d ", bootdelay);
	}

	putc ('\n');

#ifdef CONFIG_SILENT_CONSOLE
	if (abort) {
		/* permanently enable normal console output */
		gd->flags &= ~(GD_FLG_SILENT);
	} else if (gd->flags & GD_FLG_SILENT) {
		/* Restore silent console */
		console_assign (stdout, "nulldev");
		console_assign (stderr, "nulldev");
	}
#endif

	return abort;
}
# endif	/* CONFIG_AUTOBOOT_KEYED */
#endif	/* CONFIG_BOOTDELAY >= 0  */

/****************************************************************************/

static ulong my_atoi(const char *c)
{
        ulong value = 0;
        int sign = 1; 
        if( *c == '+' || *c == '-' )
        {
                if( *c == '-' ) sign = -1;
                c++;
        }
        while (isdigit(*c))
        {
                value *= 10;
                value += (int) (*c-'0');
                c++;
        }
        return (value * sign);
}

extern char *getenv (char *name);
extern void setenv (char *varname, char *varvalue);
extern int saveenv(void);

unsigned long bootcount_load(void)
{
        return my_atoi(getenv("bootcount"));
}

void bootcount_store(ulong count)
{
#ifdef CONFIG_AR300M
        char buf[10];
        sprintf(buf, "%ld", count);
        setenv("bootcount", buf);
        saveenv();
        run_command("protect off all",0);
        green_led_off();
        red_led_off();
#endif
}

extern int reset_button_status(void);

char rst_key_5s = 0;
void main_loop (void)
{
#ifndef CFG_HUSH_PARSER
	static char lastcommand[CFG_CBSIZE] = { 0, };
	int len;
	int rc = 1;
	int flag;
#endif
#ifdef GPIO_SIM_SELECT //sim select 0:sim1 1:sim2
	//printf("Function:%s Line:%d GPIO_SIM_SELECT=%d\n",__func__, __LINE__, GPIO_SIM_SELECT);
	set_gpio_value(GPIO_SIM_SELECT,0);   
#endif

#ifdef GPIO_4G1_POWER //enable 4G modem 1 power 0:enable 1:disable
	//printf("Function:%s Line:%d GPIO_4G1_POWER=%d\n",__func__, __LINE__, GPIO_4G1_POWER);
	set_gpio_value(GPIO_4G1_POWER,0);   
#endif

#ifdef GPIO_4G2_POWER //enable 4G modem 2 power 0:enable 1:disable
	set_gpio_value(GPIO_4G2_POWER,0); 
	//printf("Function:%s Line:%d GPIO_4G2_POWER=%d\n",__func__, __LINE__, GPIO_4G2_POWER);
#endif
//#ifdef GPIO_WATCHDOG1 //pull up watchdog gpio
//	set_gpio_value(GPIO_WATCHDOG1,1); 
	//printf("Function:%s Line:%d GPIO_WATCHDOG1=%d\n",__func__, __LINE__, GPIO_WATCHDOG1);
//#endif

#ifdef GPIO_WATCHDOG2
	gpio_watchdog_toggle(20);  //stop watchdog
#endif

	int counter = 0;

#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	char *s;
	int bootdelay;
#endif
#ifdef CONFIG_PREBOOT
	char *p;
#endif
#ifdef CONFIG_BOOTCOUNT_LIMIT
	unsigned long bootcount = 0;
	unsigned long bootlimit = 0;
	char *bcs;
	char bcs_set[16];
#endif /* CONFIG_BOOTCOUNT_LIMIT */

#if defined(CONFIG_VFD) && defined(VFD_TEST_LOGO)
	ulong bmp = 0;		/* default bitmap */
	extern int trab_vfd (ulong bitmap);

#ifdef CONFIG_MODEM_SUPPORT
	if (do_mdm_init)
		bmp = 1;	/* alternate bitmap */
#endif
	trab_vfd (bmp);
#endif	/* CONFIG_VFD && VFD_TEST_LOGO */

#ifdef CONFIG_MODEM_SUPPORT
	debug ("DEBUG: main_loop:   do_mdm_init=%d\n", do_mdm_init);
	if (do_mdm_init) {
		char *str = strdup(getenv("mdm_cmd"));
		setenv ("preboot", str);  /* set or delete definition */
		if (str != NULL)
			free (str);
		mdm_init(); /* wait for modem connection */
	}
#endif  /* CONFIG_MODEM_SUPPORT */

#ifdef CONFIG_VERSION_VARIABLE
	{
		extern char version_string[];

		setenv ("ver", version_string);  /* set version variable */
	}
#endif /* CONFIG_VERSION_VARIABLE */

#ifdef CFG_HUSH_PARSER
	u_boot_hush_start ();
#endif

#ifdef CONFIG_AUTO_COMPLETE
	install_auto_complete();
#endif

#ifdef CONFIG_PREBOOT
	if ((p = getenv ("preboot")) != NULL) {
# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif

# ifndef CFG_HUSH_PARSER
		run_command (p, 0);
# else
		parse_string_outer(p, FLAG_PARSE_SEMICOLON |
				    FLAG_EXIT_FROM_LOOP);
# endif

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif
	}
#endif /* CONFIG_PREBOOT */

#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	s = getenv ("bootdelay");
	bootdelay = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;

//	debug ("### main_loop entered: bootdelay=%d\n\n", bootdelay);

	if(reset_button_status()){
                // wait 0,5s
                udelay(500000);

                printf("Press reset button for at least:\n- %d sec. to run web failsafe mode\n",
                       CONFIG_DELAY_TO_AUTORUN_HTTPD);
                printf("Reset button is pressed for: %2d ", counter);

                while(reset_button_status() && counter < CONFIG_DELAY_TO_AUTORUN_HTTPD){
                        // LED ON and wait 0,15s
                        red_led_on();
                        udelay(150000);

                        // LED OFF and wait 0,85s
                        red_led_off();
                        udelay(850000);

                        counter++;

                        // how long the button is pressed?
                        printf("\b\b\b%2d ", counter);
						mifi_v3_send_msg("{ \"button\": \"%d\" }",counter);

                        //turn on Red LED to show httpd started
                        if(counter==CONFIG_DELAY_TO_AUTORUN_HTTPD){
                                green_led_on();
                        }

                        if(!reset_button_status()){
                                break;
                        }
                }

                //all_led_off();

                if(counter > 0){

                        // run web failsafe mode
                        if(counter >= CONFIG_DELAY_TO_AUTORUN_HTTPD){
                                printf("\n\nButton was pressed for %d sec...\nHTTP server is starting for firmware update...\n\n", counter); 
                                rst_key_5s = 1;
                               // run_command("dhcpd start", 0);
                                mifi_v3_send_msg("{ \"system\": \"goweb\"  }");
                                rst_key_5s = 0;
						    	NetLoopHttpd();
                                bootdelay = -1;
                        } else {
                                printf("\n\n## Error: button wasn't pressed long enough!\nContinuing normal boot...\n\n");
                        }

                } else {
                        printf("\n\n## Error: button wasn't pressed long enough!\nContinuing normal boot...\n\n");
                }

        }

# ifdef CONFIG_BOOT_RETRY_TIME
	init_cmd_timeout ();
# endif	/* CONFIG_BOOT_RETRY_TIME */

	s = getenv ("bootcmd");
       if (!s) {
#ifdef CONFIG_ROOTFS_FLASH
           /* XXX if rootfs is in flash, expect uImage to be in flash */
#ifdef CONFIG_AR7100
           setenv ("bootcmd", "bootm 0xbf200000");
#else
           setenv ("bootcmd", "bootm 0xbf450000");
#endif /* CONFIG_AR7100 */
#else
           setenv ("bootcmd", "tftpboot 0x8022c090 uImage; bootm 0x8022c090");
#endif
       }

#ifdef CONFIG_DUALIMAGE_SUPPORT
		findbdr(0);
#endif
		s = getenv ("bootcmd");

//	debug ("### main_loop: bootcmd=\"%s\"\n", s ? s : "<UNDEFINED>");
	if (bootdelay >= 0 && s && !abortboot (bootdelay)) {
		gl_probe_upgrade = 0;//停止升级检测
#ifdef CONFIG_GL_RSA
	rsa_verify_into_firmware();
#endif

#ifdef CONFIG_BOOTCOUNT_LIMIT
	bootcount = bootcount_load();
	bootcount++;
	bootcount_store (bootcount);
	sprintf (bcs_set, "%lu", bootcount);
	setenv ("bootcount", bcs_set);
	bcs = getenv ("bootlimit");
	bootlimit = bcs ? simple_strtoul (bcs, NULL, 10) : 0;

	if (bootlimit && (bootcount > bootlimit)) {
		printf ("Warning: Bootlimit (%u) exceeded. bootm nor flash.\n",
		        (unsigned)bootlimit);
		nand_boot_failed = 1;
		char s_cmd[30]={0};
		sprintf(s_cmd,"bootm 0x%x",GL_BOOT_ADDR);
		setenv("bootcmd",s_cmd);
	}
#endif // CONFIG_BOOTCOUNT_LIMIT

# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif
		int status=calibration_status();
                /*是不是需要校准
                 * -1: no art found, stop
                 * 0 : not calibrated, booting calibration firmware
                 * 1 : calibrated, and but need to write config
                 * 2 : calibrated, and config ready
                */
		char boot_cmd[30]={0};
		switch(status){
                        case 6:goto mainloop;break;
                        case 4:
                        case 5:
                        case 3:
			        bootcount_store(4);//wait for the stand frimware ready
                               printf("Booting image at: 0x%X\n",GL_BOOT_ADDR);
			       sprintf(boot_cmd,"bootm 0x%x",GL_BOOT_ADDR);
                               run_command(boot_cmd,0);
                                goto mainloop;break;
                        //case 3:run_command("run lc",0);break;
                        case 2:
                        case 0:printf("Booting image at: 0x%X\n",GL_BOOT_ADDR);break;
                        case 1:printf("Booting image at: 0x81000000\n");break;
                        default:break;
                }
        check_tftp_file();
        
#ifdef GPIO_WATCHDOG2
		gpio_watchdog_toggle(5); //start x1200 watchdog
#endif

        select_boot_dev();
        if(nand_boot_failed){
			sprintf(boot_cmd,"bootm 0x%x",GL_BOOT_ADDR);
            run_command(boot_cmd,0);
		}
# ifdef CONFIG_AR300M
		else {
			sprintf(boot_cmd,"nboot 0x81000000 0");
			run_command(boot_cmd,0);
		}
# endif

# ifndef CFG_HUSH_PARSER
		run_command (s, 0);
# else
		parse_string_outer(s, FLAG_PARSE_SEMICOLON |
				    FLAG_EXIT_FROM_LOOP);
# endif

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif

    //	 printf("\n## Error: failed to boot linux !\nHTTPD server is starting...##\n\n");
    //   run_command("dhcpd start", 0);
# ifdef CONFIG_XE300
    	exception_led_indicator();
# endif
        run_command("dhcpd start", 0);
	    NetLoopHttpd();
        bootdelay = -1;
    }

# ifdef CONFIG_MENUKEY
	if (menukey == CONFIG_MENUKEY) {
	    s = getenv("menucmd");
	    if (s) {
# ifndef CFG_HUSH_PARSER
		run_command (s, 0);
# else
		parse_string_outer(s, FLAG_PARSE_SEMICOLON |
				    FLAG_EXIT_FROM_LOOP);
# endif
	    }
	}
#endif /* CONFIG_MENUKEY */
#endif	/* CONFIG_BOOTDELAY */

#ifdef CONFIG_AMIGAONEG3SE
	{
	    extern void video_banner(void);
	    video_banner();
	}
#endif

	/*
	 * Main Loop for Monitor Command Processing
	 */
mainloop:
#ifdef CONFIG_GL_RSA
	rsa_verify_into_uboot_console();
#endif

#ifdef CFG_HUSH_PARSER
	parse_file_outer();
	/* This point is never reached */
	for (;;);
#else
	for (;;) {
#ifdef CONFIG_BOOT_RETRY_TIME
		if (rc >= 0) {
			/* Saw enough of a valid command to
			 * restart the timeout.
			 */
			reset_cmd_timeout();
		}
#endif
		len = readline (CFG_PROMPT);

		flag = 0;	/* assume no special flags for now */
		if (len > 0)
			strcpy (lastcommand, console_buffer);
		else if (len == 0)
			flag |= CMD_FLAG_REPEAT;
#ifdef CONFIG_BOOT_RETRY_TIME
		else if (len == -2) {
			/* -2 means timed out, retry autoboot
			 */
			puts ("\nTimed out waiting for command\n");
# ifdef CONFIG_RESET_TO_RETRY
			/* Reinit board to run initialization code again */
			do_reset (NULL, 0, 0, NULL);
# else
			return;		/* retry autoboot */
# endif
		}
#endif

		if (len == -1)
			puts ("<INTERRUPT>\n");
		else
			rc = run_command (lastcommand, flag);

		if (rc <= 0) {
			/* invalid command or not repeatable, forget it */
			lastcommand[0] = 0;
		}
	}
#endif /*CFG_HUSH_PARSER*/
}

#ifdef CONFIG_BOOT_RETRY_TIME
/***************************************************************************
 * initialise command line timeout
 */
void init_cmd_timeout(void)
{
	char *s = getenv ("bootretry");

	if (s != NULL)
		retry_time = (int)simple_strtol(s, NULL, 10);
	else
		retry_time =  CONFIG_BOOT_RETRY_TIME;

	if (retry_time >= 0 && retry_time < CONFIG_BOOT_RETRY_MIN)
		retry_time = CONFIG_BOOT_RETRY_MIN;
}

/***************************************************************************
 * reset command line timeout to retry_time seconds
 */
void reset_cmd_timeout(void)
{
	endtime = endtick(retry_time);
}
#endif

/****************************************************************************/

/*
 * Prompt for input and read a line.
 * If  CONFIG_BOOT_RETRY_TIME is defined and retry_time >= 0,
 * time out when time goes past endtime (timebase time in ticks).
 * Return:	number of read characters
 *		-1 if break
 *		-2 if timed out
 */
int readline (const char *const prompt)
{
	char   *p = console_buffer;
	int	n = 0;				/* buffer index		*/
	int	plen = 0;			/* prompt length	*/
	int	col;				/* output column cnt	*/
	char	c;

	/* print prompt */
	if (prompt) {
		plen = strlen (prompt);
		puts (prompt);
	}
	col = plen;

	for (;;) {
#ifdef CONFIG_BOOT_RETRY_TIME
		while (!tstc()) {	/* while no incoming data */
			if (retry_time >= 0 && get_ticks() > endtime)
				return (-2);	/* timed out */
		}
#endif
		WATCHDOG_RESET();		/* Trigger watchdog, if needed */

#ifdef CONFIG_SHOW_ACTIVITY
		while (!tstc()) {
			extern void show_activity(int arg);
			show_activity(0);
		}
#endif
		c = getc();

		/*
		 * Special character handling
		 */
		switch (c) {
		case '\r':				/* Enter		*/
		case '\n':
			*p = '\0';
			puts ("\r\n");
			return (p - console_buffer);

		case '\0':				/* nul			*/
			continue;

		case 0x03:				/* ^C - break		*/
			console_buffer[0] = '\0';	/* discard input */
			return (-1);

		case 0x15:				/* ^U - erase line	*/
			while (col > plen) {
				puts (erase_seq);
				--col;
			}
			p = console_buffer;
			n = 0;
			continue;

		case 0x17:				/* ^W - erase word 	*/
			p=delete_char(console_buffer, p, &col, &n, plen);
			while ((n > 0) && (*p != ' ')) {
				p=delete_char(console_buffer, p, &col, &n, plen);
			}
			continue;

		case 0x08:				/* ^H  - backspace	*/
		case 0x7F:				/* DEL - backspace	*/
			p=delete_char(console_buffer, p, &col, &n, plen);
			continue;

		default:
			/*
			 * Must be a normal character then
			 */
			if (n < CFG_CBSIZE-2) {
				if (c == '\t') {	/* expand TABs		*/
#ifdef CONFIG_AUTO_COMPLETE
					/* if auto completion triggered just continue */
					*p = '\0';
					if (cmd_auto_complete(prompt, console_buffer, &n, &col)) {
						p = console_buffer + n;	/* reset */
						continue;
					}
#endif
					puts (tab_seq+(col&07));
					col += 8 - (col&07);
				} else {
					++col;		/* echo input		*/
					putc (c);
				}
				*p++ = c;
				++n;
			} else {			/* Buffer full		*/
				putc ('\a');
			}
		}
	}
}

/****************************************************************************/

static char * delete_char (char *buffer, char *p, int *colp, int *np, int plen)
{
	char *s;

	if (*np == 0) {
		return (p);
	}

	if (*(--p) == '\t') {			/* will retype the whole line	*/
		while (*colp > plen) {
			puts (erase_seq);
			(*colp)--;
		}
		for (s=buffer; s<p; ++s) {
			if (*s == '\t') {
				puts (tab_seq+((*colp) & 07));
				*colp += 8 - ((*colp) & 07);
			} else {
				++(*colp);
				putc (*s);
			}
		}
	} else {
		puts (erase_seq);
		(*colp)--;
	}
	(*np)--;
	return (p);
}

/****************************************************************************/

int parse_line (char *line, char *argv[])
{
	int nargs = 0;

#ifdef DEBUG_PARSER
	printf ("parse_line: \"%s\"\n", line);
#endif
	while (nargs < CFG_MAXARGS) {

		/* skip any white space */
		while ((*line == ' ') || (*line == '\t')) {
			++line;
		}

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
#ifdef DEBUG_PARSER
		printf ("parse_line: nargs=%d\n", nargs);
#endif
			return (nargs);
		}

		argv[nargs++] = line;	/* begin of argument string	*/

		/* find end of string */
		while (*line && (*line != ' ') && (*line != '\t')) {
			++line;
		}

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
#ifdef DEBUG_PARSER
		printf ("parse_line: nargs=%d\n", nargs);
#endif
			return (nargs);
		}

		*line++ = '\0';		/* terminate current arg	 */
	}

	printf ("** Too many args (max. %d) **\n", CFG_MAXARGS);

#ifdef DEBUG_PARSER
	printf ("parse_line: nargs=%d\n", nargs);
#endif
	return (nargs);
}

/****************************************************************************/

static void process_macros (const char *input, char *output)
{
	char c, prev;
	const char *varname_start = NULL;
	int inputcnt  = strlen (input);
	int outputcnt = CFG_CBSIZE;
	int state = 0;	/* 0 = waiting for '$'	*/
			/* 1 = waiting for '(' or '{' */
			/* 2 = waiting for ')' or '}' */
			/* 3 = waiting for '''  */
#ifdef DEBUG_PARSER
	char *output_start = output;

	printf ("[PROCESS_MACROS] INPUT len %d: \"%s\"\n", strlen(input), input);
#endif

	prev = '\0';			/* previous character	*/

	while (inputcnt && outputcnt) {
	    c = *input++;
	    inputcnt--;

	    if (state!=3) {
	    /* remove one level of escape characters */
	    if ((c == '\\') && (prev != '\\')) {
		if (inputcnt-- == 0)
			break;
		prev = c;
		c = *input++;
	    }
	    }

	    switch (state) {
	    case 0:			/* Waiting for (unescaped) $	*/
		if ((c == '\'') && (prev != '\\')) {
			state = 3;
			break;
		}
		if ((c == '$') && (prev != '\\')) {
			state++;
		} else {
			*(output++) = c;
			outputcnt--;
		}
		break;
	    case 1:			/* Waiting for (	*/
		if (c == '(' || c == '{') {
			state++;
			varname_start = input;
		} else {
			state = 0;
			*(output++) = '$';
			outputcnt--;

			if (outputcnt) {
				*(output++) = c;
				outputcnt--;
			}
		}
		break;
	    case 2:			/* Waiting for )	*/
		if (c == ')' || c == '}') {
			int i;
			char envname[CFG_CBSIZE], *envval;
			int envcnt = input-varname_start-1; /* Varname # of chars */

			/* Get the varname */
			for (i = 0; i < envcnt; i++) {
				envname[i] = varname_start[i];
			}
			envname[i] = 0;

			/* Get its value */
			envval = getenv (envname);

			/* Copy into the line if it exists */
			if (envval != NULL)
				while ((*envval) && outputcnt) {
					*(output++) = *(envval++);
					outputcnt--;
				}
			/* Look for another '$' */
			state = 0;
		}
		break;
	    case 3:			/* Waiting for '	*/
		if ((c == '\'') && (prev != '\\')) {
			state = 0;
		} else {
			*(output++) = c;
			outputcnt--;
		}
		break;
	    }
	    prev = c;
	}

	if (outputcnt)
		*output = 0;

#ifdef DEBUG_PARSER
	printf ("[PROCESS_MACROS] OUTPUT len %d: \"%s\"\n",
		strlen(output_start), output_start);
#endif
}

/****************************************************************************
 * returns:
 *	1  - command executed, repeatable
 *	0  - command executed but not repeatable, interrupted commands are
 *	     always considered not repeatable
 *	-1 - not executed (unrecognized, bootd recursion or too many args)
 *           (If cmd is NULL or "" or longer than CFG_CBSIZE-1 it is
 *           considered unrecognized)
 *
 * WARNING:
 *
 * We must create a temporary copy of the command since the command we get
 * may be the result from getenv(), which returns a pointer directly to
 * the environment data, which may change magicly when the command we run
 * creates or modifies environment variables (like "bootp" does).
 */

int run_command (const char *cmd, int flag)
{
	cmd_tbl_t *cmdtp;
	char cmdbuf[CFG_CBSIZE];	/* working copy of cmd		*/
	char *token;			/* start of token in cmdbuf	*/
	char *sep;			/* end of token (separator) in cmdbuf */
	char finaltoken[CFG_CBSIZE];
	char *str = cmdbuf;
	char *argv[CFG_MAXARGS + 1];	/* NULL terminated	*/
	int argc, inquotes;
	int repeatable = 1;
	int rc = 0;

#ifdef DEBUG_PARSER
	printf ("[RUN_COMMAND] cmd[%p]=\"", cmd);
	puts (cmd ? cmd : "NULL");	/* use puts - string may be loooong */
	puts ("\"\n");
#endif

	clear_ctrlc();		/* forget any previous Control C */

	if (!cmd || !*cmd) {
		return -1;	/* empty command */
	}

	if (strlen(cmd) >= CFG_CBSIZE) {
		puts ("## Command too long!\n");
		return -1;
	}

	strcpy (cmdbuf, cmd);

	/* Process separators and check for invalid
	 * repeatable commands
	 */

#ifdef DEBUG_PARSER
	printf ("[PROCESS_SEPARATORS] %s\n", cmd);
#endif
	while (*str) {

		/*
		 * Find separator, or string end
		 * Allow simple escape of ';' by writing "\;"
		 */
		for (inquotes = 0, sep = str; *sep; sep++) {
			if ((*sep=='\'') &&
			    (*(sep-1) != '\\'))
				inquotes=!inquotes;

			if (!inquotes &&
			    (*sep == ';') &&	/* separator		*/
			    ( sep != str) &&	/* past string start	*/
			    (*(sep-1) != '\\'))	/* and NOT escaped	*/
				break;
		}
		/*
		 * Limit the token to data between separators
		 */
		token = str;
		if (*sep) {
			str = sep + 1;	/* start of command for next pass */
			*sep = '\0';
		}
		else
			str = sep;	/* no more commands for next pass */
#ifdef DEBUG_PARSER
		printf ("token: \"%s\"\n", token);
#endif

		/* find macros in this token and replace them */
		process_macros (token, finaltoken);

		/* Extract arguments */
		if ((argc = parse_line (finaltoken, argv)) == 0) {
			rc = -1;	/* no command at all */
			continue;
		}

		/* Look up command in command table */
		if ((cmdtp = find_cmd(argv[0])) == NULL) {
			printf ("Unknown command '%s' - try 'help'\n", argv[0]);
			rc = -1;	/* give up after bad command */
			continue;
		}

		/* found - check max args */
		if (argc > cmdtp->maxargs) {
			printf ("Usage:\n%s\n", cmdtp->usage);
			rc = -1;
			continue;
		}

#if (CONFIG_COMMANDS & CFG_CMD_BOOTD)
		/* avoid "bootd" recursion */
		if (cmdtp->cmd == do_bootd) {
#ifdef DEBUG_PARSER
			printf ("[%s]\n", finaltoken);
#endif
			if (flag & CMD_FLAG_BOOTD) {
				puts ("'bootd' recursion detected\n");
				rc = -1;
				continue;
			} else {
				flag |= CMD_FLAG_BOOTD;
			}
		}
#endif	/* CFG_CMD_BOOTD */

		/* OK - call function to do the command */
		if ((cmdtp->cmd) (cmdtp, flag, argc, argv) != 0) {
			rc = -1;
		}

		repeatable &= cmdtp->repeatable;

		/* Did the user stop this? */
		if (had_ctrlc ())
			return 0;	/* if stopped then not repeatable */
	}

	return rc ? rc : repeatable;
}

/****************************************************************************/

#if (CONFIG_COMMANDS & CFG_CMD_RUN)
int do_run (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int i;

	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	for (i=1; i<argc; ++i) {
		char *arg;

		if ((arg = getenv (argv[i])) == NULL) {
			printf ("## Error: \"%s\" not defined\n", argv[i]);
			return 1;
		}
#ifndef CFG_HUSH_PARSER
		if (run_command (arg, flag) == -1)
			return 1;
#else
		if (parse_string_outer(arg,
		    FLAG_PARSE_SEMICOLON | FLAG_EXIT_FROM_LOOP) != 0)
			return 1;
#endif
	}
	return 0;
}
#endif	/* CFG_CMD_RUN */
