#include <common.h>
#include <command.h>
#include <asm/io.h>

#define LED_GPIO_OE     0x18040000
#define LED_GPIO_IN     0x18040004
#define LED_GPIO_OUT    0x18040008
#define LED_GPIO_SET    0x1804000c
#define LED_GPIO_CLEAR  0x18040010

char *led_name[3] = {"pwrled","2gled","5gled"};

static int set_pin(int pin)
{
	unsigned int led_GPIO_OE  = ath_reg_rd(LED_GPIO_OE);
	led_GPIO_OE  &= ~(1<<pin);
	ath_reg_wr(LED_GPIO_OE,led_GPIO_OE);
	ath_reg_wr_nf(LED_GPIO_SET,(unsigned int)(1<<pin));
	return 0;
}

static int clear_pin(int pin)
{
	unsigned int led_GPIO_OE  = ath_reg_rd(LED_GPIO_OE);
        led_GPIO_OE  &= ~(1<<pin);      
        ath_reg_wr(LED_GPIO_OE,led_GPIO_OE);
        ath_reg_wr_nf(LED_GPIO_CLEAR,(unsigned int)(1<<pin));
	return 0;

}


static int set_led(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i;
	if(argc != 3){
		printf("Usage:\n	- setled name value\n");
		return -1;
	}
	if(strcmp(argv[2],"1")&&strcmp(argv[2],"0")){
		printf("bad value\n");
		return -1;
	}
	if(strcmp(argv[1],led_name[0]) \
		&& strcmp(argv[1],led_name[1]) \
		&& strcmp(argv[1],led_name[2])){
		printf("bad led\n");
		return -1;
	}
	for(i=0;i<=2;i++)
		if(!strcmp(led_name[i],argv[1]))
			break;
	switch(i)
	{
		case 0:strcmp(argv[2],"0")?set_pin(1):clear_pin(1);break;
		case 1:strcmp(argv[2],"0")?set_pin(19):clear_pin(19);break;
		case 2:strcmp(argv[2],"0")?set_pin(20):clear_pin(20);break;
		default:printf("ERROR!\n");return -1;break;	
	}
	return 0;
}

U_BOOT_CMD(
	setled,3,1,set_led,
	"setled	- turn off/on led\n",
	" name value ...\n	-setled <pwrled | 2gled | 5gled> <1 | 0>\n "
);
