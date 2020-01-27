#include <SDL/SDL.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <strings.h>
#include "keydef.h"
#include "dingoo.h"

#include "snes9x.h"
#include "memmap.h"
#include "cpuexec.h"
#include "snapshot.h"
#include "display.h"
#include "gfx.h"
#include "unistd.h"

#include "sdlaudio.h"

#ifdef WIN32
static void sync() { }
#endif

extern Uint16 sfc_key[256];
extern bool8_32 Scale;
extern char SaveSlotNum;
extern short vol;

extern void S9xDisplayString (const char *string, uint8 *, uint32, int ypos);
void save_screenshot(char *fname);
void load_screenshot(char *fname);
void show_screenshot(void);
void capt_screenshot(void);
void menu_dispupdate(void);
void ShowCredit(void);
int CurrentButton(int);
int NectButton(int);
int PrevButton(int);

int cursor = 2;
int loadcursor = 0;
int romcount_maxrows = 16;
int remapbuttons = 0;
char SaveSlotNum_old=255;
bool8_32 highres_current = FALSE;
char snapscreen[17120]={};

char temp[256];
char disptxt[20][256];

void sys_sleep(int us)
{
	if(us>0)
		SDL_Delay(us/100);
}

extern SDL_Surface *screen, *gfxscreen;

void menu_flip()
{
	SDL_Rect dst;

	dst.x = (screen->w - 256) / 2;
	dst.y = (screen->h - 224) / 2;
	SDL_BlitSurface(gfxscreen, NULL, screen, &dst);
	SDL_Flip(screen);
}

#ifndef DINGOO
//------------------------------------------------------------------------------------------
struct dirent **namelist;

int isFile(const struct dirent *nombre) {
 int isFile = 0;
 char *extension = rindex( (char*) nombre->d_name, '.');
 if (strcmp(extension, ".sfc") == 0 ||
 	 strcmp(extension, ".smc") == 0 ||
 	 strcmp(extension, ".zip" ) == 0 )
 {
  isFile = 1;
 }

 return isFile;
}

int FileDir(char *dir, const char *ext)
{
	int n;

	//printf ("Try to create ./roms directory..");
#ifdef WIN32
	mkdir (dir);
#else
	mkdir (dir, 0777);
	chown (dir, getuid (), getgid ());
#endif

	n = scandir (dir, &namelist, isFile, alphasort);
	if (n >= 0)
	{
//		int cnt;
//		for (cnt = 0; cnt < n; ++cnt)
//			puts (namelist[cnt]->d_name);
	}
	else
	{
		perror ("Couldn't open ./roms directory..try to create this directory");
	#ifdef WIN32
		mkdir (dir);
	#else
		mkdir (dir, 0777);
		chown (dir, getuid (), getgid ());
	#endif
		n = scandir (dir, &namelist, isFile, alphasort);
	}
		
	return n;
}

void loadmenu_dispupdate(int romcount)
{
	//draw blue screen
	for(int y=12;y<=212;y++){
		for(int x=10;x<246*2;x+=2){
			memset(GFX.Screen + GFX.Pitch*y+x,0x11,2);
		}	
	}

#if CAANOO
	strcpy(disptxt[0],"  Snes9x4C v20101010");
#elif CYGWIN32
	strcpy(disptxt[0],"  Snes9x4W v20101010");
#else
	sprintf(disptxt[0],"  Snes9x4D for OpenDingux build %d",__DATE__);
#endif

	//copy roms filenames to disp[] cache
	for(int i=0;i<=romcount_maxrows;i++)
	{
		if (loadcursor>romcount_maxrows)
		{
			if((i+(loadcursor-romcount_maxrows))==loadcursor)
				sprintf(temp," >%s",namelist[ i+(loadcursor-romcount_maxrows) ]->d_name);
			else
				sprintf(temp,"  %s",namelist[ i+(loadcursor-romcount_maxrows) ]->d_name);

			strncpy(disptxt[i+2],temp,34);
			disptxt[i+2][34]='\0';
		}
		else
		if (i<romcount)
		{
			if(i==loadcursor)
				sprintf(temp," >%s",namelist[i]->d_name);
			else
				sprintf(temp,"  %s",namelist[i]->d_name);

			strncpy(disptxt[i+2],temp,34);
			disptxt[i+2][34]='\0';
		}
	}

	//draw 20 lines on screen
	for(int i=0;i<19;i++)
	{
		S9xDisplayString (disptxt[i], GFX.Screen, GFX.Pitch, i*10+64);
	}

	//update screen
	menu_flip();
}

char* menu_romselector()
{
	char *rom_filename = NULL;
	int romcount = 0;

	bool8_32 exit_loop = false;

#ifdef CAANOO
	SDL_Joystick* keyssnes = 0;
#else
	uint8 *keyssnes = 0;
#endif

	//Read ROM-Directory
	romcount = FileDir("./roms", "sfc,smc");

	highres_current=Settings.SupportHiRes;

	Settings.SupportHiRes=FALSE;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);
	
	loadmenu_dispupdate(romcount);
	sys_sleep(10000);

	SDL_Event event;

	do
	{
		loadmenu_dispupdate(romcount);
		sys_sleep(100);

		while(SDL_PollEvent(&event)==1)
		{
			// DINGOO & WIN32 -----------------------------------------------------
			keyssnes = SDL_GetKeyState(NULL);
			switch(event.type)
			{
				case SDL_KEYDOWN:
					keyssnes = SDL_GetKeyState(NULL);

					//UP
					if(keyssnes[sfc_key[UP_1]] == SDL_PRESSED)
						loadcursor--;
					//DOWN
					else if(keyssnes[sfc_key[DOWN_1]] == SDL_PRESSED)
						loadcursor++;
//					//LS
//					else if(keyssnes[sfc_key[L_1]] == SDL_PRESSED)
//						loadcursor=loadcursor-10;
//					//RS
//					else if(keyssnes[sfc_key[R_1]] == SDL_PRESSED)
//						loadcursor=loadcursor+10;
					//QUIT Emulator : press ESCAPE KEY
					else if (keyssnes[sfc_key[SELECT_1]] == SDL_PRESSED)
						S9xExit();
					else if( (keyssnes[sfc_key[B_1]] == SDL_PRESSED) )
					{
						switch(loadcursor)
						{
							default:
								if ((keyssnes[sfc_key[B_1]] == SDL_PRESSED))
								{
									if ((loadcursor>=0) && (loadcursor<(romcount)))
									{
										rom_filename=namelist[loadcursor]->d_name;
										exit_loop = TRUE;
									}
								}
								break;
						}
					}
					break;
			}

			if(loadcursor==-1)
			{
				loadcursor=romcount-1;
			}
			else
			if(loadcursor==romcount)
			{
				loadcursor=0;
			}
			break;
		}
	}

	while( exit_loop!=TRUE && keyssnes[sfc_key[B_1]] != SDL_PRESSED );

	// TODO:
	///free(). 	namelist

	Settings.SupportHiRes=highres_current;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);

	return (rom_filename);
}

//------------------------------------------------------------------------------------------
#endif // DINGOO

void menu_dispupdate(void)
{
	char *Rates[8] = { "  OFF", " 8192", "11025", "16000", "22050", "32000", "44100", "48000" };
    char *Button[15] ={\
        "[A]B L R STA SEL",\
        " A[B]L R STA SEL", "  NONE","  NONE",\
        " A B[L]R STA SEL",\
        " A B L[R]STA SEL", "  NONE","  NONE","  NONE","  NONE","  NONE","  NONE",\
        " A B L R[STA]SEL",\
        " A B L R STA[SEL]",\
        " A B L R STA SEL "};
    
	//memset(GFX.Screen + 320*12*2,0x11,320*200*2);
	for(int y=12;y<=212;y++){
		for(int x=10;x<246*2;x+=2){
			memset(GFX.Screen + GFX.Pitch*y+x,0x11,2);
		}	
	}
    if(remapbuttons==0){
        sprintf(disptxt[0],"Snes9x4D for RS-90");
        strcpy(disptxt[1],"----------------------------");
        strcpy(disptxt[2],"Reset Game           ");
        strcpy(disptxt[3],"Save State           ");
        strcpy(disptxt[4],"Load State           ");
        sprintf(disptxt[5],"State Slot              No.%d",SaveSlotNum);
        sprintf(disptxt[6],"Show FPS                 %s",(Settings.DisplayFrameRate ? " ON":"OFF"));
        sprintf(disptxt[7],"Transparency             %s",(Settings.Transparency ? " ON":"OFF"));
        sprintf(disptxt[8],"Video Mode          %s",(Scale ? "224 Line" : "208 Line"));
        strcpy(disptxt[9],"Frameskip              ");
        sprintf(disptxt[10],"Sound Rate             %s",Rates[Settings.SoundPlaybackRate]);
        strcpy(disptxt[11],"Remap Buttons        ");
        strcpy(disptxt[12],"Credit              ");
        strcpy(disptxt[13],"Exit Game");

        if (Settings.SkipFrames == AUTO_FRAMERATE)
            sprintf(temp,"%s AUTO",disptxt[9]);
        else
            sprintf(temp,"%s %02d/%d",disptxt[9],(int) Memory.ROMFramesPerSecond, Settings.SkipFrames);
        strcpy(disptxt[9],temp);
    }
    else{
        sprintf(disptxt[0],"Remap Buttons");
        strcpy(disptxt[1], "----------------------------");
        strcpy(disptxt[2],  "[SNES]         [RS-90]");
        sprintf(disptxt[3], "A Button      %s",Button[CurrentButton(sfc_key[A_1])]);
        sprintf(disptxt[4], "B Button      %s",Button[CurrentButton(sfc_key[B_1])]);
        sprintf(disptxt[5], "X Button      %s",Button[CurrentButton(sfc_key[X_1])]);
        sprintf(disptxt[6], "Y Button      %s",Button[CurrentButton(sfc_key[Y_1])]);
        sprintf(disptxt[7], "L Button      %s",Button[CurrentButton(sfc_key[L_1])]);
        sprintf(disptxt[8], "R Button      %s",Button[CurrentButton(sfc_key[R_1])]);
        sprintf(disptxt[9], "START Button  %s",Button[CurrentButton(sfc_key[START_1])]);
        sprintf(disptxt[10],"SELECT Button %s",Button[CurrentButton(sfc_key[SELECT_1])]);
        sprintf(disptxt[11],"Reset to Default    ");        
        sprintf(disptxt[12], "       ");
        strcpy(disptxt[13], "Return to Menu         ");
    }
                
    for(int i=0;i<=13;i++)
	{
		if(i==cursor)
			sprintf(temp,"  >%s",disptxt[i]);
		else
			sprintf(temp,"   %s",disptxt[i]);
		strcpy(disptxt[i],temp);

		S9xDisplayString (disptxt[i], GFX.Screen, GFX.Pitch ,i*10+84);
	}

	//show screen shot for snapshot
	if(SaveSlotNum_old != SaveSlotNum)
	{
		//strcpy(temp,"LOADING...");
		//S9xDisplayString (temp, GFX.Screen + 280, GFX.Pitch, 210);
		menu_flip();
		char fname[256], ext[8];
		sprintf(ext, ".s0%d", SaveSlotNum);
		strcpy(fname, S9xGetFilename (ext));
		load_screenshot(fname);
		SaveSlotNum_old = SaveSlotNum;
	}
    if(remapbuttons==0){
        if(cursor==3 | cursor==4 | cursor==5) 
            show_screenshot();
    }
	menu_flip();
}

/*
#define A_1 0
#define B_1 1
#define X_1 2
#define Y_1 3
#define L_1 4
#define R_1 5
#define START_1 12
#define SELECT_1 13
#define DINGOO_BUTTON_R             8
#define DINGOO_BUTTON_L             9
#define DINGOO_BUTTON_A             306
#define DINGOO_BUTTON_B             308
#define DINGOO_BUTTON_X             32
#define DINGOO_BUTTON_Y             304
#define DINGOO_BUTTON_SELECT        27
#define DINGOO_BUTTON_START         13
#define DINGOO_BUTTON_END           0
*/
int CurrentButton(int button){
    int ret=14;
    if (button==DINGOO_BUTTON_A)
            ret=0;
    else if(button==DINGOO_BUTTON_B)
        ret=1;
    else if(button==DINGOO_BUTTON_L)
        ret=4;
    else if(button==DINGOO_BUTTON_R)
        ret=5;
    else if(button==DINGOO_BUTTON_START)
        ret=12;
    else if(button==DINGOO_BUTTON_SELECT)
        ret=13;

    return ret;
}

int NextButton(int button){
    int ret=button;
    if(button==DINGOO_BUTTON_A)
        ret = DINGOO_BUTTON_B;
    else if(button==DINGOO_BUTTON_B)
        ret =DINGOO_BUTTON_L;
    else if (button==DINGOO_BUTTON_L)
            ret = DINGOO_BUTTON_R;
    else if(button==DINGOO_BUTTON_R)
        ret = DINGOO_BUTTON_START;
    else if(button==DINGOO_BUTTON_START)
        ret = DINGOO_BUTTON_SELECT;
    else if(button==DINGOO_BUTTON_SELECT)
        ret = 14;
    else
        ret = DINGOO_BUTTON_A;
    return ret;
}

int PrevButton(int button){
    int ret=button;
    if(button==DINGOO_BUTTON_A)
        ret = 14;
    else if(button==DINGOO_BUTTON_B)
        ret =DINGOO_BUTTON_A;
    else if (button==DINGOO_BUTTON_L)
            ret = DINGOO_BUTTON_B;
    else if(button==DINGOO_BUTTON_R)
        ret = DINGOO_BUTTON_L;
    else if(button==DINGOO_BUTTON_START)
        ret = DINGOO_BUTTON_R;
    else if(button==DINGOO_BUTTON_SELECT)
        ret = DINGOO_BUTTON_START;
    else
        ret = DINGOO_BUTTON_SELECT;
    return ret;
}

void menu_loop(void)
{
	int old_frame_rate = Settings.SoundPlaybackRate;
	bool8_32 exit_loop = false;
	char fname[256], ext[8];
	char snapscreen_tmp[17120];

	uint8 *keyssnes = 0;

	SaveSlotNum_old = -1;

	highres_current=Settings.SupportHiRes;

	capt_screenshot();
	memcpy(snapscreen_tmp,snapscreen,17120);

	Settings.SupportHiRes=FALSE;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);

	menu_dispupdate();
	sys_sleep(10000);

	SDL_Event event;

	do
	{
		while(SDL_PollEvent(&event)==1)
		{
				keyssnes = SDL_GetKeyState(NULL);
            
				if(keyssnes[DINGOO_BUTTON_UP] == SDL_PRESSED)
					cursor--;
				else if(keyssnes[DINGOO_BUTTON_DOWN] == SDL_PRESSED)
					cursor++;
                else if ((keyssnes[DINGOO_BUTTON_B] == SDL_PRESSED))
                {
                    if(remapbuttons){
                        cursor =11;
                        remapbuttons=0;
                        do{
                            SDL_Event event;
                            SDL_PollEvent(&event);
                            keyssnes = SDL_GetKeyState(NULL);
                            sys_sleep(10000);
                        }
                        while(keyssnes[DINGOO_BUTTON_B] == SDL_PRESSED);
                    }
                }
				else if( (keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED) ||
						 (keyssnes[DINGOO_BUTTON_LEFT] == SDL_PRESSED) ||
						 (keyssnes[DINGOO_BUTTON_RIGHT] == SDL_PRESSED) )
				{
                    if(remapbuttons==0){
                        switch(cursor)
                        {
						case 2:
							if ((keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED))
							{
								S9xReset();
								exit_loop = TRUE;
							}
						break;
						case 3:
							if (keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED)
							{
								memcpy(snapscreen,snapscreen_tmp,16050);
								show_screenshot();
								strcpy(fname," SAVING...");
								S9xDisplayString (fname, GFX.Screen +280, GFX.Pitch, 204);
								menu_flip();
								sprintf(ext, ".s0%d", SaveSlotNum);
								strcpy(fname, S9xGetFilename (ext));
								save_screenshot(fname);
								sprintf(ext, ".00%d", SaveSlotNum);
								strcpy(fname, S9xGetFilename (ext));
								S9xFreezeGame (fname);
								sync();
								exit_loop = TRUE;
							}
						break;
						case 4:
							if (keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED)
							{
								sprintf(ext, ".00%d", SaveSlotNum);
								strcpy(fname, S9xGetFilename (ext));
								S9xLoadSnapshot (fname);
								exit_loop = TRUE;
							}
						break;
						case 5:
							if (keyssnes[DINGOO_BUTTON_LEFT] == SDL_PRESSED)
								SaveSlotNum--;
							else
							if (keyssnes[DINGOO_BUTTON_RIGHT] == SDL_PRESSED)
								SaveSlotNum++;

							if(SaveSlotNum>=3)
								SaveSlotNum=3;
							else if(SaveSlotNum<=0)
								SaveSlotNum=0;
						break;
						case 6: //Toggle Show FPS
							Settings.DisplayFrameRate = !Settings.DisplayFrameRate;
						break;
						case 7: //Toggle Transparency
							Settings.Transparency = !Settings.Transparency;
						break;
						case 8: //Toggle Video Mode
							Scale = !Scale;
						break;
						case 9:
							if (Settings.SkipFrames == AUTO_FRAMERATE)
								Settings.SkipFrames = 10;
	
							if (keyssnes[DINGOO_BUTTON_LEFT] == SDL_PRESSED)
								Settings.SkipFrames--;
							else
								Settings.SkipFrames++;
	
							if(Settings.SkipFrames>=10)
								Settings.SkipFrames = AUTO_FRAMERATE;
							else if (Settings.SkipFrames<=1)
								Settings.SkipFrames = 1;
						break;
						case 10:
							if (keyssnes[DINGOO_BUTTON_LEFT] == SDL_PRESSED) {
								Settings.SoundPlaybackRate = (Settings.SoundPlaybackRate - 1) & 7;
							} else if (keyssnes[DINGOO_BUTTON_RIGHT] == SDL_PRESSED) {
								Settings.SoundPlaybackRate = (Settings.SoundPlaybackRate + 1) & 7;
							}
						break;
						case 11: //Goto Remap Button Menu
							if (keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED)
                            {
                                cursor = 2;
								remapbuttons=1;
                            }
                        break;
						case 12:
							if (keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED)
								ShowCredit();
						break;
						case 13:
							if (keyssnes[DINGOO_BUTTON_A] == SDL_PRESSED)
								S9xExit();
						break;
                        } //switch(cursor)
                    }
                    else{ //in RemapButton menu   
                        switch(cursor)
                        {
                        #define SETKEY(KEY) \
                        {\
                            if (keyssnes[DINGOO_BUTTON_LEFT] == SDL_PRESSED) \
                                sfc_key[KEY]=PrevButton(sfc_key[KEY]);\
                            else sfc_key[KEY]=NextButton(sfc_key[KEY]);\
                        }
                        case 3: //A Button
                            SETKEY(A_1);
                        break;
                        case 4: //B Button
                            SETKEY(B_1);
                        break;
                        case 5: //X Button
                            SETKEY(X_1);
                        break;
                        case 6: //Y Button
                            SETKEY(Y_1);
                        break;
                        case 7: //L Button
                             SETKEY(L_1);
                       break;
                        case 8: //R Button
                            SETKEY(R_1);
                        break;
                        case 9: //START Button
                             SETKEY(START_1);
                       break;
                        case 10: //SELECT Button
                            SETKEY(SELECT_1);
                        break;
                        case 11://reset to default
                            sfc_key[A_1] = DINGOO_BUTTON_A;
                            sfc_key[B_1] = DINGOO_BUTTON_B;
                            sfc_key[X_1] = DINGOO_BUTTON_X;
                            sfc_key[Y_1] = DINGOO_BUTTON_Y;
                            sfc_key[L_1] = DINGOO_BUTTON_L;
                            sfc_key[R_1] = DINGOO_BUTTON_R;
                            sfc_key[START_1] = DINGOO_BUTTON_START;
                            sfc_key[SELECT_1] = DINGOO_BUTTON_SELECT;
                        break;
                        case 13: //exit to menu
                            cursor=11;
                            remapbuttons=0;
                        break;
                        }
                    }
				}

				if(cursor==1)
					cursor=13;	//11
				else if(cursor==14)	//12
					cursor=2;
				
				menu_dispupdate();
				sys_sleep(1000);

				break;
		}
	}
	while( exit_loop!=TRUE && keyssnes[DINGOO_BUTTON_B] != SDL_PRESSED );

	Settings.SupportHiRes=highres_current;
	S9xDeinitDisplay();
	S9xInitDisplay(0, 0);
	if(old_frame_rate != Settings.SoundPlaybackRate) S9xReinitSound(Settings.SoundPlaybackRate);
}

void save_screenshot(char *fname){
	FILE  *fs = fopen (fname,"wb");
	if(fs==NULL)
		return;
	
	fwrite(snapscreen,17120,1,fs);
	fclose (fs);
	sync();
}

void load_screenshot(char *fname)
{
	FILE *fs = fopen (fname,"rb");
	if(fs==NULL){
		for(int i=0;i<17120;i++){
			snapscreen[i] = 0;
		}
		return;
	}
	fread(snapscreen,17120,1,fs);
	fclose(fs);
}

void capt_screenshot() //107px*80px
{
	bool8_32 Scale_disp=Scale;
	int s = 0;
	int yoffset = 0;
	struct InternalPPU *ippu = &IPPU;

	for(int i=0;i<17120;i++)
	{
		snapscreen[i] = 0x00;
	}

	if(ippu->RenderedScreenHeight == 224)
		yoffset = 8;

	if (highres_current==TRUE)
	{
		//working but in highres mode
		for(int y=yoffset;y<240-yoffset;y+=3) //80,1 //240,3
		{
			s += 22 * 1 /*(Scale_disp!=TRUE)*/;
			for(int x = 0; x < 640-128*1/*(Scale_disp!=TRUE)*/;x+=6) //107,1 //214,2 //428,4 +42+42
			{
				uint8 *d = GFX.Screen + y*GFX.Pitch + x; //1024
				snapscreen[s++] = *d++;
				snapscreen[s++] = *d++;
			}
			s+=20*1/*(Scale_disp!=TRUE)*/;
		}
	}
	else
	{
		//original
		for(int y=yoffset;y<240-yoffset;y+=3) // 240/3=80
		{
			s+=22*(Scale_disp!=TRUE);
			for(int x=0 ;x<640-128*(Scale_disp!=TRUE);x+=3*2) // 640/6=107
			{
				uint8 *d = GFX.Screen + y*GFX.Pitch + x;
				snapscreen[s++] = *d++;
				snapscreen[s++] = *d++;
			}
			s+=20*(Scale_disp!=TRUE);
		}
	}
}

void show_screenshot()
{
	int s=0;
//	for(int y=126;y<126+80;y++){
	for(int y=132;y<130+80;y++){
		for(int x=248; x<248+107*2; x+=2){
			uint8 *d = GFX.Screen + y*GFX.Pitch + x;
			*d++ = snapscreen[s++];
			*d++ = snapscreen[s++];
		}
	}
}

void ShowCredit()
{
#ifdef CAANOO
	SDL_Joystick* keyssnes = 0;
#else
	uint8 *keyssnes = 0;
#endif
	int line=0,ypix=0;
	char disptxt[100][256]={
	"",
	"",
	"",
	"",
	"",
	"                                     ",
	"  Thank you for playing Snes9X4D!",
	"                                     ",
	"[overwritten below]",
	"",
	"",
	"",
	"",
	"",
	"",
	"   by Drowsnug",
	"     drowsnug95.wordpress.com",
	"",
	"  regards to joyrider & SiENcE &      ",
	"                   dmitrysmagin      ",
	"",
	};
    sprintf(disptxt[8],"   (build on %s %s)",__DATE__,__TIME__);
    
	do
	{
		SDL_Event event;
		SDL_PollEvent(&event);

#ifdef CAANOO
		keyssnes = SDL_JoystickOpen(0);
#else
		keyssnes = SDL_GetKeyState(NULL);
#endif
		for(int y=12; y<=212; y++){
			for(int x=10; x<246*2; x+=2){
				memset(GFX.Screen + GFX.Pitch*y+x,0x11,2);
			}	
		}
		
		for(int i=0;i<=16;i++){
			int j=i+line;
			if(j>=20) j-=20;
			S9xDisplayString (disptxt[j], GFX.Screen, GFX.Pitch, i*10+80-ypix);
		}
		
		ypix+=2;
		if(ypix==12) {
			line++;
			ypix=0;
		}
		if(line == 20) line = 0;
		menu_flip();
		sys_sleep(10000);
	}
#ifdef CAANOO
	while( SDL_JoystickGetButton(keyssnes, sfc_key[B_1])!=TRUE );
#else
	while(keyssnes[DINGOO_BUTTON_B] != SDL_PRESSED);
#endif
    
    do{
        SDL_Event event;
		SDL_PollEvent(&event);
        keyssnes = SDL_GetKeyState(NULL);
        sys_sleep(10000);
    }
    while(keyssnes[DINGOO_BUTTON_B] == SDL_PRESSED);
    
	return;
}

