#ifndef SMALL
#ifndef NO_STBIMAGE

#include "openiboot.h"
#include "lcd.h"
#include "util.h"
#include "framebuffer.h"
#include "buttons.h"
#include "timer.h"
#include "images/ConsolePNG.h"
#include "images/iPhoneOSPNG.h"
#include "images/AndroidOSPNG.h"
#include "images/ConsoleSelectedPNG.h"
#include "images/iPhoneOSSelectedPNG.h"
#include "images/AndroidOSSelectedPNG.h"
#include "images/HeaderPNG.h"
#include "images.h"
#include "actions.h"
#include "stb_image.h"
#include "pmu.h"
#include "nand.h"
#include "radio.h"
#include "hfs/fs.h"
#include "ftl.h"
#include "scripting.h"
#include "multitouch.h"
#include "nvram.h"

#define CHAINLOAD 1
#define CONSOLE 2
#define LINUX 3

int globalFtlHasBeenRestored; /* global variable to tell wether a ftl_restore has been done*/

static uint32_t FBWidth;
static uint32_t FBHeight;


static uint32_t* imgiPhoneOS;
static int imgiPhoneOSWidth;
static int imgiPhoneOSHeight;
static int imgiPhoneOSX;
static int imgiPhoneOSY;

static uint32_t* imgConsole;
static int imgConsoleWidth;
static int imgConsoleHeight;
static int imgConsoleX;
static int imgConsoleY;

static uint32_t* imgAndroidOS;
static int imgAndroidOSWidth;
static int imgAndroidOSHeight;
static int imgAndroidOSX;
static int imgAndroidOSY;

static uint32_t* imgiPhoneOSSelected;
static uint32_t* imgConsoleSelected;
static uint32_t* imgAndroidOSSelected;

static uint32_t* imgHeader;
static int imgHeaderWidth;
static int imgHeaderHeight;
static int imgHeaderX;
static int imgHeaderY;

int numOptions;

typedef struct {
	int type;
	int uid;
	char img3image[5];
	char title[64];
	char kernel[255];
	char ramdisk[255];
	char console[255];
	char init[255];
	char rootfs[255];
} menuOption;

int selection;

volatile uint32_t* OtherFramebuffer;

static void drawSelectionBox() {
	volatile uint32_t* oldFB = CurFramebuffer;

	CurFramebuffer = OtherFramebuffer;
	currentWindow->framebuffer.buffer = CurFramebuffer;
	OtherFramebuffer = oldFB;

	if(selection == 0) {
		framebuffer_draw_image(imgiPhoneOSSelected, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOS, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
	}

	if(selection == 1) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOSSelected, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
	}

	if(selection == 2) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsoleSelected, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOS, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
	}

	lcd_window_address(2, (uint32_t) CurFramebuffer);
}
/* Die multitouch!
static int touch_watcher()
{
    uint8_t isFound = 0;
    multitouch_run();
    if (multitouch_ispoint_inside_region(imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight) == TRUE) {
        Selection = MenuSelectioniPhoneOS;
        isFound = 1;
    }
    else if (multitouch_ispoint_inside_region(imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight) == TRUE) {
        Selection = MenuSelectionConsole;
        isFound = 1;
    }
    else if (multitouch_ispoint_inside_region(imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight) == TRUE) {
        Selection = MenuSelectionAndroidOS;
        isFound = 1;
    }
    
    if (isFound ==1) {
        drawSelectionBox();
        return TRUE;
    }
    return FALSE;
}*/

static void toggle(int forward) {
	if(forward)
	{
		if(selection == numOptions) {
			selection = 0;
		} else {
			selection++;
		}
	} else {
		if(selection == 0) {
			selection = numOptions;
		} else {
			selection--;
		}
	}

	drawSelectionBox();
}

int parse_menu_option(int option, menuOption *thisOption) {
	char sOption = (char)(option+48);

	char opibType[12] = "opib-type-";
	char opibUid[11] = "opib-uid-";
	char opibTitle[13] = "opib-title-";
	char opibImg3[17] = "opib-img3image-";
	char opibKernel[14] = "opib-kernel-";
	char opibRamdisk[15] = "opib-ramdisk-";
	char opibConsole[15] = "opib-console-";
	char opibInit[12] = "opib-init-";
	char opibRootFS[12] = "opib-root-";

	opibType[10] = sOption;
	const char *sType = nvram_getvar(opibType);
	int type = parseNumber(sType);
	thisOption[option].type = type;

	opibUid[9] = sOption;
	const char *sUid = nvram_getvar(opibUid);
	thisOption[option].uid = parseNumber(sUid);

	opibTitle[11] = sOption;
	const char *sTitle = nvram_getvar(opibTitle);
	strcpy(thisOption[option].title, sTitle);

	switch(type) {
		case CHAINLOAD:
			opibImg3[15] = sOption;
			const char *sImg3 = nvram_getvar(opibImg3);
			strcpy(thisOption[option].img3image, sImg3);
			break;
		case CONSOLE:
			break;
		case LINUX:
			opibKernel[12] = sOption;
			const char *sKernel = nvram_getvar(opibKernel);
			strcpy(thisOption[option].kernel, sKernel);

			opibRamdisk[13] = sOption;
			const char *sRamdisk = nvram_getvar(opibRamdisk);
			strcpy(thisOption[option].ramdisk, sRamdisk);

			opibConsole[13] = sOption;
			const char *sConsole = nvram_getvar(opibConsole);
			strcpy(thisOption[option].console, sConsole);

			opibInit[10] = sOption;
			const char *sInit = nvram_getvar(opibInit);
			strcpy(thisOption[option].init, sInit);
			
			opibRootFS[10] = sOption;
			const char *sRootFS = nvram_getvar(opibRootFS);
			strcpy(thisOption[option].rootfs, sRootFS);
			break;
		default:
			return -1;
	}

	//DEBUG:
	/*
	printf("Type: %d\n", thisOption[option].type);
	printf("UID: %d\n", thisOption[option].uid);
	printf("Title: %s\n", thisOption[option].title);
	printf("IMG3: %s\n", thisOption[option].img3image);
	printf("Kernel: %s\n", thisOption[option].kernel);
	printf("Ramdisk: %s\n", thisOption[option].ramdisk);
	printf("Flags: console=%s init=%s root=%s\n\n\n", thisOption[option].console, thisOption[option].init, thisOption[option].rootfs);
	*/
	return 0;
}

int menu_setup(int timeout, int defaultOS) {
	//DEBUG:
	framebuffer_setdisplaytext(TRUE);

	const char *sNumOptions = nvram_getvar("opib-options");
	numOptions = parseNumber(sNumOptions);
	int i;

	printf("Options: %i\n", numOptions);
	
	menuOption menuConfig[numOptions];	

	for(i=1;i<(numOptions+1);i++) {
		parse_menu_option(i, menuConfig);
	}

	//Debug x,y co-ords (will be set by theme file eventually)
	int titlexloc[] = {5,5,5};
	int titleyloc[] = {5,25,35};

	int imgxloc[] = {5,5,5};
	int imgyloc[] = {84,207,330};

	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;
	
	framebuffer_setcolors(COLOR_WHITE, COLOR_BLACK);

	for(i=0;i<numOptions;i++) {
		framebuffer_setloc(titlexloc[i], titleyloc[i]);
		framebuffer_print(menuConfig[i+1].title);

		
	}

	framebuffer_setloc(0, 0);

	//Old stuff:

	imgiPhoneOS = framebuffer_load_image(dataiPhoneOSPNG, dataiPhoneOSPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);
	imgiPhoneOSSelected = framebuffer_load_image(dataiPhoneOSSelectedPNG, dataiPhoneOSSelectedPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);

	/* Stopping compile bugs for now cba to remove */
	imgConsole = framebuffer_load_image(dataConsolePNG, dataConsolePNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	imgConsoleSelected = framebuffer_load_image(dataConsoleSelectedPNG, dataConsoleSelectedPNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	imgAndroidOS = framebuffer_load_image(dataAndroidOSPNG, dataAndroidOSPNG_size, &imgAndroidOSWidth, &imgAndroidOSHeight, TRUE);
	imgAndroidOSSelected = framebuffer_load_image(dataAndroidOSSelectedPNG, dataAndroidOSSelectedPNG_size, &imgAndroidOSWidth, &imgAndroidOSHeight, TRUE);
	imgHeader = framebuffer_load_image(dataHeaderPNG, dataHeaderPNG_size, &imgHeaderWidth, &imgHeaderHeight, TRUE);
	

	//bufferPrintf("menu: images loaded\r\n");

	imgiPhoneOSX = imgxloc[0];
	imgiPhoneOSY = imgyloc[0];

	imgAndroidOSX = imgxloc[1];
	imgAndroidOSY = imgyloc[1];

	imgConsoleX = imgxloc[2];
	imgConsoleY = imgyloc[2];

	imgHeaderX = (FBWidth - imgHeaderWidth) / 2;
	imgHeaderY = 17;

	framebuffer_draw_image(imgHeader, imgHeaderX, imgHeaderY, imgHeaderWidth, imgHeaderHeight);

	/* Not important for now
	framebuffer_setloc(0, 47);
	framebuffer_setcolors(COLOR_WHITE, COLOR_BLACK);
	framebuffer_print_force(OPENIBOOT_VERSION_STR);
	framebuffer_setloc(0, 0);
	*/

	/* Old stuff for ref
	switch(defaultOS){
		case 0:
			Selection = MenuSelectioniPhoneOS;
			break;
		case 1:
			Selection = MenuSelectionAndroidOS;
			break;
		case 2:
			Selection = MenuSelectionConsole;
			break;
		default:
			Selection = MenuSelectioniPhoneOS;
	}*/

	//selection = defaultOS;
	selection = 0;

	OtherFramebuffer = CurFramebuffer;
	CurFramebuffer = (volatile uint32_t*) NextFramebuffer;

	drawSelectionBox();

	pmu_set_iboot_stage(0);

	memcpy((void*)NextFramebuffer, (void*) CurFramebuffer, NextFramebuffer - (uint32_t)CurFramebuffer);
	
	
	uint64_t startTime = timer_get_system_microtime();
/*Disable timeout for now
	int timeoutLeft = (timeout / 1000);
	*/
		

	while(TRUE) {
		/*Timeout/MT disable
		char timeoutstr[4] = "";
		if(timeout > 0){
			sprintf(timeoutstr, "%2d", timeoutLeft);
			uint64_t checkTime = timer_get_system_microtime();
			timeoutLeft = (timeout - ((checkTime - startTime)/1000))/1000;
			framebuffer_setloc(49, 47);
			framebuffer_print_force(timeoutstr);
			framebuffer_setloc(0,0);
			drawSelectionBox();
		} else if(timeout == -1) {
			timeoutLeft = -1;
			sprintf(timeoutstr, "  ");
			framebuffer_setloc(49, 47);
			framebuffer_print_force(timeoutstr);
			framebuffer_setloc(0,0);
			drawSelectionBox();
		}
		if (isMultitouchLoaded && touch_watcher()) {
			break;
        	}*/
		if(buttons_is_pushed(BUTTONS_HOLD)) {
			toggle(TRUE);
			startTime = timer_get_system_microtime();
			timeout = -1;
			udelay(200000);
		}
#ifndef CONFIG_IPOD
		if(!buttons_is_pushed(BUTTONS_VOLUP)) {
			toggle(FALSE);
			startTime = timer_get_system_microtime();
			timeout = -1;
			udelay(200000);
		}
		if(!buttons_is_pushed(BUTTONS_VOLDOWN)) {
			toggle(TRUE);
			startTime = timer_get_system_microtime();
			timeout = -1;
			udelay(200000);
		}
#endif
		if(buttons_is_pushed(BUTTONS_HOME)) {
			timeout = -1;
			break;
		}
		/*
		if(timeout > 0 && has_elapsed(startTime, (uint64_t)timeout * 1000)) {
			bufferPrintf("menu: timed out, selecting current item\r\n");
			break;
		}
		*/
		udelay(10000);
	}

	//C doesn't like declarations in switch statements grr!
	Image* image;
	void* imageData;

	switch(menuConfig[selection+1].type) {
		case CHAINLOAD:
			image = images_get(fourcc(menuConfig[selection+1].img3image));
			if(strcmp(menuConfig[selection+1].img3image,"ibox") != 0 && image == NULL) {
				image = images_get(fourcc("ibot"));	//We check this if oib isn't already installed
			}
			images_read(image, &imageData);
			chainload((uint32_t)imageData);
			break;

		case CONSOLE:
			if((uint32_t) CurFramebuffer == NextFramebuffer)
			{
				CurFramebuffer = OtherFramebuffer;
				currentWindow->framebuffer.buffer = CurFramebuffer;
				lcd_window_address(2, (uint32_t) CurFramebuffer);
			}

			framebuffer_setdisplaytext(TRUE);
			framebuffer_clear();
			break;

		case LINUX:
			// Reset framebuffer back to original if necessary
			if((uint32_t) CurFramebuffer == NextFramebuffer)
			{
				CurFramebuffer = OtherFramebuffer;
				currentWindow->framebuffer.buffer = CurFramebuffer;
				lcd_window_address(2, (uint32_t) CurFramebuffer);
			}

			framebuffer_setdisplaytext(TRUE);
			framebuffer_clear();

#ifndef NO_HFS
#ifndef CONFIG_IPOD
			radio_setup();
#endif
			nand_setup();
			fs_setup();
			if(globalFtlHasBeenRestored) /* if ftl has been restored, sync it, so kernel doesn't have to do a ftl_restore again */
			{
				if(ftl_sync())
				{
					bufferPrintf("ftl synced successfully");
				}
				else
				{
					bufferPrintf("error syncing ftl");
				}
			}

			pmu_set_iboot_stage(0);
			startScripting("linux"); //start script mode if there is a script file
			boot_linux_from_files(); //Replace me!!!
#endif
			break;

		default:
			//Chainload iOS as backup
			image = images_get(fourcc("ibox"));
			if(image == NULL) {
				image = images_get(fourcc("ibot"));
			}
			images_read(image, &imageData);
			chainload((uint32_t)imageData);
			break;
	}

	return 0;
}

#endif
#endif

