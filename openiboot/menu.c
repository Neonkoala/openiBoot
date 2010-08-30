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

int globalFtlHasBeenRestored; /* global variable to tell wether a ftl_restore has been done*/

static uint32_t FBWidth;
static uint32_t FBHeight;

static uint32_t* imgHeader;
static int imgHeaderWidth;
static int imgHeaderHeight;
static int imgHeaderX;
static int imgHeaderY;

typedef enum MenuSelection {
	MenuSelectioniPhoneOS,
	MenuSelectionAndroidOS,
	MenuSelectionConsole
} MenuSelection;

typedef struct MenuImage {
    uint32_t size;
    uint32_t *image;
    uint16_t x;
    uint16_t y;
    int w;
    int h;
} MenuImage;

typedef struct MenuItem {
    uint8_t osId;
    MenuImage imageNormal;
    MenuImage imageFocused;
} MenuItem;

typedef struct MenuTheme {
    uint32_t backgroundSize;
    uint32_t *background;
    uint8_t totalMenu;
    MenuItem **menus;
} MenuTheme;

static int SelectionIndex;
static MenuTheme *menuTheme;
static int totalDefaultMenuItem=3;

volatile uint32_t* OtherFramebuffer;

static void drawSelectionBox() {
	volatile uint32_t* oldFB = CurFramebuffer;

	CurFramebuffer = OtherFramebuffer;
	currentWindow->framebuffer.buffer = CurFramebuffer;
	OtherFramebuffer = oldFB;

    int ii=0;
    for (ii=0;ii<menuTheme->totalMenu;ii++) {
        if (SelectionIndex == ii) {
            framebuffer_draw_image(menuTheme->menus[ii]->imageFocused.image, menuTheme->menus[ii]->imageFocused.x, menuTheme->menus[ii]->imageFocused.y
            , menuTheme->menus[ii]->imageFocused.w, menuTheme->menus[ii]->imageFocused.h);
        }else {
            framebuffer_draw_image(menuTheme->menus[ii]->imageNormal.image, menuTheme->menus[ii]->imageNormal.x, menuTheme->menus[ii]->imageNormal.y
            , menuTheme->menus[ii]->imageNormal.w, menuTheme->menus[ii]->imageNormal.h);
        }
    }    

	lcd_window_address(2, (uint32_t) CurFramebuffer);
}

static int touch_watcher()
{
    int ii=0;
    multitouch_run();
    
    for (ii=0;ii<menuTheme->totalMenu;ii++) {
        if (SelectionIndex == ii) {
            //check against focused image
            if (multitouch_ispoint_inside_region(menuTheme->menus[ii]->imageFocused.x, menuTheme->menus[ii]->imageFocused.y, 
                menuTheme->menus[ii]->imageFocused.w, menuTheme->menus[ii]->imageFocused.h) == TRUE) {
                SelectionIndex=ii;
                drawSelectionBox();
                return TRUE;
            }
        } else {
            if (multitouch_ispoint_inside_region(menuTheme->menus[ii]->imageNormal.x, menuTheme->menus[ii]->imageNormal.y, 
                menuTheme->menus[ii]->imageNormal.w, menuTheme->menus[ii]->imageNormal.h) == TRUE) {
                SelectionIndex=ii;
                drawSelectionBox();
                return TRUE;
            }
        }
    }  
    
    return FALSE;
}

static void toggle(int forward) {
    if (forward) {
        SelectionIndex++;
        if (SelectionIndex>=menuTheme->totalMenu) {
            SelectionIndex=0;
        }
    } else {
        SelectionIndex--;
        if (SelectionIndex<0) {
            SelectionIndex=menuTheme->totalMenu-1;
        }
    }

	drawSelectionBox();
}

static void menuDefaultSetup() {
    
    menuTheme = (MenuTheme *) malloc(sizeof(MenuTheme));
    menuTheme->totalMenu = totalDefaultMenuItem;
    
    menuTheme->menus = (MenuItem **) malloc(sizeof(MenuItem*) * menuTheme->totalMenu);
    
    //iOs
    menuTheme->menus[0] = (MenuItem *) malloc(sizeof(MenuItem));
    menuTheme->menus[0]->osId = MenuSelectioniPhoneOS;
    menuTheme->menus[0]->imageNormal.size = dataiPhoneOSPNG_size;
    menuTheme->menus[0]->imageNormal.image = framebuffer_load_image(dataiPhoneOSPNG, menuTheme->menus[0]->imageNormal.size, &(menuTheme->menus[0]->imageNormal.w), &(menuTheme->menus[0]->imageNormal.h), TRUE);
    menuTheme->menus[0]->imageNormal.x = (FBWidth - menuTheme->menus[0]->imageNormal.w) / 2; 
    menuTheme->menus[0]->imageNormal.y = 84;
    menuTheme->menus[0]->imageFocused.size = dataiPhoneOSSelectedPNG_size;
    menuTheme->menus[0]->imageFocused.image = framebuffer_load_image(dataiPhoneOSSelectedPNG, menuTheme->menus[0]->imageFocused.size, &(menuTheme->menus[0]->imageFocused.w), &(menuTheme->menus[0]->imageFocused.h), TRUE);
    menuTheme->menus[0]->imageFocused.x = (FBWidth - menuTheme->menus[0]->imageFocused.w) / 2;
    menuTheme->menus[0]->imageFocused.y = 84;
    
    //AndroidOS
    menuTheme->menus[1] = (MenuItem *) malloc(sizeof(MenuItem));
    menuTheme->menus[1]->osId = MenuSelectionAndroidOS;
    menuTheme->menus[1]->imageNormal.size = dataAndroidOSPNG_size;
    menuTheme->menus[1]->imageNormal.image = framebuffer_load_image(dataAndroidOSPNG, menuTheme->menus[1]->imageNormal.size, &(menuTheme->menus[1]->imageNormal.w), &(menuTheme->menus[1]->imageNormal.h), TRUE);
    menuTheme->menus[1]->imageNormal.x = (FBWidth - menuTheme->menus[1]->imageNormal.w) / 2; 
    menuTheme->menus[1]->imageNormal.y = 207;
    menuTheme->menus[1]->imageFocused.size = dataAndroidOSSelectedPNG_size;
    menuTheme->menus[1]->imageFocused.image = framebuffer_load_image(dataAndroidOSSelectedPNG, menuTheme->menus[1]->imageFocused.size, &(menuTheme->menus[1]->imageFocused.w), &(menuTheme->menus[1]->imageFocused.h), TRUE);
    menuTheme->menus[1]->imageFocused.x = (FBWidth - menuTheme->menus[1]->imageFocused.w) / 2;
    menuTheme->menus[1]->imageFocused.y = 207;
    
    //Console
    menuTheme->menus[2] = (MenuItem *) malloc(sizeof(MenuItem));
    menuTheme->menus[2]->osId = MenuSelectionConsole;
    menuTheme->menus[2]->imageNormal.size = dataConsolePNG_size;
    menuTheme->menus[2]->imageNormal.image = framebuffer_load_image(dataConsolePNG, menuTheme->menus[2]->imageNormal.size, &(menuTheme->menus[2]->imageNormal.w), &(menuTheme->menus[2]->imageNormal.h), TRUE);
    menuTheme->menus[2]->imageNormal.x = (FBWidth - menuTheme->menus[2]->imageNormal.w) / 2; 
    menuTheme->menus[2]->imageNormal.y = 330;
    menuTheme->menus[2]->imageFocused.size = dataConsoleSelectedPNG_size;
    menuTheme->menus[2]->imageFocused.image = framebuffer_load_image(dataConsoleSelectedPNG, menuTheme->menus[2]->imageFocused.size, &(menuTheme->menus[2]->imageFocused.w), &(menuTheme->menus[2]->imageFocused.h), TRUE);
    menuTheme->menus[2]->imageFocused.x = (FBWidth - menuTheme->menus[2]->imageFocused.w) / 2;
    menuTheme->menus[2]->imageFocused.y = 330;
        
    imgHeader = framebuffer_load_image(dataHeaderPNG, dataHeaderPNG_size, &imgHeaderWidth, &imgHeaderHeight, TRUE);
    
    imgHeaderX = (FBWidth - imgHeaderWidth) / 2;
    imgHeaderY = 17;
    
    framebuffer_draw_image(imgHeader, imgHeaderX, imgHeaderY, imgHeaderWidth, imgHeaderHeight);
}

int menuThemeSetup() 
{
}

int menu_setup(int timeout, int defaultOS) {
	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;	
    
    menuDefaultSetup();

    //setup text based menu
    /*framebuffer_setloc(130, 20);
    framebuffer_setcolors(COLOR_WHITE, 0x228252);
    framebuffer_print_force(" iOS ");
    
    framebuffer_setloc(128, 25);
    framebuffer_setcolors(COLOR_WHITE, 0x228252);
    framebuffer_print_force(" Android \r");
    
    framebuffer_setloc(128, 30);
    framebuffer_setcolors(COLOR_WHITE, 0x228252);
    framebuffer_print_force(" Console \r");    */
    
	framebuffer_setloc(0, 47);
	framebuffer_setcolors(COLOR_WHITE, COLOR_BLACK);
	framebuffer_print_force(OPENIBOOT_VERSION_STR);
	framebuffer_setloc(0, 0);
    SelectionIndex=defaultOS;	

	OtherFramebuffer = CurFramebuffer;
	CurFramebuffer = (volatile uint32_t*) NextFramebuffer;

	drawSelectionBox();

	pmu_set_iboot_stage(0);

	memcpy((void*)NextFramebuffer, (void*) CurFramebuffer, NextFramebuffer - (uint32_t)CurFramebuffer);

	uint64_t startTime = timer_get_system_microtime();
	int timeoutLeft = (timeout / 1000);
	while(TRUE) {
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
        }
        	
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
		if(timeout > 0 && has_elapsed(startTime, (uint64_t)timeout * 1000)) {
			bufferPrintf("menu: timed out, selecting current item\r\n");
			break;
		}
		udelay(10000);
	}
    
    int osId = menuTheme->menus[SelectionIndex]->osId;
    
    if(osId == MenuSelectioniPhoneOS) {
		Image* image = images_get(fourcc("ibox"));
		if(image == NULL)
			image = images_get(fourcc("ibot"));
		void* imageData;
		images_read(image, &imageData);
		chainload((uint32_t)imageData);
	}

    if(osId == MenuSelectionConsole) {
		// Reset framebuffer back to original if necessary
		if((uint32_t) CurFramebuffer == NextFramebuffer)
		{
			CurFramebuffer = OtherFramebuffer;
			currentWindow->framebuffer.buffer = CurFramebuffer;
			lcd_window_address(2, (uint32_t) CurFramebuffer);
		}

		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();
	}

    if(osId == MenuSelectionAndroidOS) {
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
		boot_linux_from_files();
#endif
	}

	return 0;
}

#endif
#endif

