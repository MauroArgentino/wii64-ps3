/**
 * Mupen64 - pif.c
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include "../../config.h"
#include <ppu-types.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../main/winlnxdefs.h"

#include "memory.h"
#include "pif.h"
#include "pif2.h"
#include "../r4300/r4300.h"
#include "../r4300/interrupt.h"
#include "../../main/plugin.h"
#include "../../main/guifuncs.h"
#include "../../main/rom.h"
#include "../../ui/fileBrowser/fileBrowser.h"
#include "Saves.h"

static unsigned char eeprom[0x800] __attribute__((aligned(32)));
static unsigned char mempack[4][0x8000] __attribute__((aligned(32)));

BOOL eepromWritten = FALSE;
BOOL mempakWritten = FALSE;
#define EEP_MC_OFFSET 0x1000

void check_input_sync(unsigned char *value);

int loadEeprom(fileBrowser_file* savepath){
	int i, result = 0;
	fileBrowser_file saveFile;
	memcpy(&saveFile, savepath, sizeof(fileBrowser_file));
	memset(&saveFile.name[0],0,FILE_BROWSER_MAX_PATH_LEN);
	sprintf((char*)saveFile.name,"%s/%s%s.eep",savepath->name,ROM_SETTINGS.goodname,saveregionstr());

	if(saveFile_readFile(&saveFile, &i, 4) == 4) {  //file exists
		saveFile.offset = 0;
		if(saveFile_readFile(&saveFile, eeprom, 0x800)!=0x800) { //error reading file
  		for (i=0; i<0x800; i++) eeprom[i] = 0;
  		eepromWritten = FALSE;
  		return -1;
		}
		result = 1;
		eepromWritten = 1;
		return result;  //file read ok
	} else for (i=0; i<0x800; i++) eeprom[i] = 0; //file doesn't exist

	eepromWritten = FALSE;

	return result;  //no file
}

extern long long gettime();
// Note: must be called after load
int saveEeprom(fileBrowser_file* savepath){
  if(!eepromWritten) return 0;
	fileBrowser_file saveFile;
	memcpy(&saveFile, savepath, sizeof(fileBrowser_file));
	memset(&saveFile.name[0],0,FILE_BROWSER_MAX_PATH_LEN);
	sprintf((char*)saveFile.name,"%s/%s%s.eep",savepath->name,ROM_SETTINGS.goodname,saveregionstr());

	if(saveFile_writeFile(&saveFile, eeprom, 0x800)!=0x800)
	  return -1;

	return 1;

}

void init_eeprom() {
  int i;
  for (i=0; i<0x800; i++) eeprom[i] = 0;
}

//#define DEBUG_PIF
#ifdef DEBUG_PIF
void print_pif()
{
   int i;
   for (i=0; i<(64/8); i++)
     printf("%x %x %x %x | %x %x %x %x\n",
	    PIF_RAMb[i*8+0], PIF_RAMb[i*8+1],PIF_RAMb[i*8+2], PIF_RAMb[i*8+3],
	    PIF_RAMb[i*8+4], PIF_RAMb[i*8+5],PIF_RAMb[i*8+6], PIF_RAMb[i*8+7]);
   getchar();
}
#endif

void EepromCommand(BYTE *Command)
{
   switch (Command[2])
     {
      case 0: // check
	if (Command[1] != 3)
	  {
	     Command[1] |= 0x40;
	     if ((Command[1] & 3) > 0)
	       Command[3] = 0;
	     if ((Command[1] & 3) > 1)
	       Command[4] = ROM_SETTINGS.eeprom_16kb == 0 ? 0x80 : 0xc0;
	     if ((Command[1] & 3) > 2)
	       Command[5] = 0;
	  }
	else
	  {
	     Command[3] = 0;
	     Command[4] = ROM_SETTINGS.eeprom_16kb == 0 ? 0x80 : 0xc0;
	     Command[5] = 0;
	  }
	break;
      case 4: // read
	  {
	     memcpy(&Command[4], eeprom + Command[3]*8, 8);
	  }
	break;
      case 5: // write
	  {
	     eepromWritten = TRUE;
	     memcpy(eeprom + Command[3]*8, &Command[4], 8);
	  }
	break;
/*      default:
    break;
*///	printf("unknown command in EepromCommand : %x\n", Command[2]);
     }
}

void format_mempacks()
{
   unsigned char init[] =
     {
	0x81,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
	0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
	0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x71,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03
     };
   int i,j;
   for (i=0; i<4; i++)
     {
	for (j=0; j<0x8000; j+=2)
	  {
	     mempack[i][j] = 0;
	     mempack[i][j+1] = 0x03;
	  }
	memcpy(mempack[i], init, 272);
     }
}

unsigned char mempack_crc(unsigned char *data)
{
   int i;
   unsigned char CRC = 0;
   for (i=0; i<=0x20; i++)
     {
	int mask;
	for (mask = 0x80; mask >= 1; mask >>= 1)
	  {
	     int xor_tap = (CRC & 0x80) ? 0x85 : 0x00;
	     CRC <<= 1;
	     if (i != 0x20 && (data[i] & mask)) CRC |= 1;
	     CRC ^= xor_tap;
	  }
     }
   return CRC;
}

int loadMempak(fileBrowser_file* savepath){
	int i, result = 0;
  fileBrowser_file saveFile;

	memcpy(&saveFile, savepath, sizeof(fileBrowser_file));
	memset(&saveFile.name[0],0,FILE_BROWSER_MAX_PATH_LEN);
	sprintf((char*)saveFile.name,"%s/%s%s.mpk",savepath->name,ROM_SETTINGS.goodname,saveregionstr());

	if(saveFile_readFile(&saveFile, &i, 4) == 4) {  //file exists
		saveFile.offset = 0;
		if(saveFile_readFile(&saveFile, mempack, 0x8000 * 4)!=(0x8000*4)) { //error reading file
  		format_mempacks();
	    mempakWritten = FALSE;
	    return -1;
    }
		result = 1;
		mempakWritten = 1;
		return result;  //file read ok
	} else format_mempacks(); //file doesn't exist

	mempakWritten = FALSE;

	return result;    //no file
}

int saveMempak(fileBrowser_file* savepath){
  if(!mempakWritten) return 0;
	fileBrowser_file saveFile;
	memcpy(&saveFile, savepath, sizeof(fileBrowser_file));
	memset(&saveFile.name[0],0,FILE_BROWSER_MAX_PATH_LEN);
	sprintf((char*)saveFile.name,"%s/%s%s.mpk",savepath->name,ROM_SETTINGS.goodname,saveregionstr());

	if(saveFile_writeFile(&saveFile, mempack, 0x8000 * 4)!=(0x8000*4))
	  return -1;

	return 1;
}

void internal_ReadController(int Control, BYTE *Command)
{
   static int pifrd_cnt = 0;
   static u32 pifrd_skip = 0;
   if (pifrd_cnt < 8 || (pifrd_cnt < 200 && (pifrd_skip++ % 100) == 0))
     {
	pifrd_cnt++;
	printf("[PIFRD#%d] ctl=%d pres=%d tx=%d rx=%d cmd=0x%02X c3=0x%02X\n",
		pifrd_cnt, Control, Controls[Control].Present,
		Command[0], Command[1], Command[2], Command[3]);
     }
   switch (Command[2])
     {
      case 1:
	if (Controls[Control].Present)
	  {
	     BUTTONS Keys;
	     Command[1] = 0x04;
	     getKeys(Control, &Keys);
	     *((u32 *)(Command + 3)) = Keys.Value;
	     {
		static int gk_cnt = 0;
		if (gk_cnt < 20 || (gk_cnt < 200 && (gk_cnt++ % 100) == 0))
		  {
		     gk_cnt++;
		     printf("[READ_BTN] ctl=%d val=%08X\n",
			    Control, (unsigned int)Keys.Value);
		  }
	     }
	  }
	else
	  Command[1] = 0x84;
	break;
      case 2: // read controller pack
	if (Controls[Control].Present)
	  {
	     Command[1] = 0x21;
	     if (Controls[Control].Plugin == PLUGIN_RAW)
	       if (controllerCommand != NULL) readController(Control, Command);
	  }
	else
	  Command[1] = 0xA1;
	break;
      case 3: // write controller pack
	if (Controls[Control].Present)
	  {
	     Command[1] = 0x01;
	     if (Controls[Control].Plugin == PLUGIN_RAW)
	       if (controllerCommand != NULL) readController(Control, Command);
	  }
	else
	  Command[1] = 0x81;
	break;
     }
}

void internal_ControllerCommand(int Control, BYTE *Command)
{
   switch (Command[2])
     {
      case 0x00: // check (REQUEST_STATUS / RESET)
      case 0xFF:
	if ((Command[1] & 0x80))
	  break;
	if (Controls[Control].Present)
	  {
	     {
		static int icc_cnt = 0;
		if (icc_cnt < 6)
		  {
		     icc_cnt++;
		     printf("[ICC_CMD0] Control=%d RawData=%d Plugin=%d\n",
			    Control, Controls[Control].RawData,
			    Controls[Control].Plugin);
		  }
	     }
	     Command[1] = 0x03;
	     Command[3] = 0x05;
	     Command[4] = 0x00;
	     switch(Controls[Control].Plugin)
	       {
		case PLUGIN_MEMPAK:
		  Command[5] = 1;
		  break;
		case PLUGIN_RAW:
		  Command[5] = 1;
		  break;
		default:
		  Command[5] = 0;
		  break;
	       }
	  }
	else
	  Command[1] = 0x83;
	break;
      case 0x01:
	if (!Controls[Control].Present)
	  Command[1] = 0x84;
	else
	  Command[1] = 0x04;
	break;
      case 0x02: // read controller pack
	if (Controls[Control].Present)
	  {
	     Command[1] = 0x21;
	     switch(Controls[Control].Plugin)
	       {
		case PLUGIN_MEMPAK:
		    {
		       int address = (Command[3] << 8) | Command[4];
		       if (address == 0x8001)
			 {
			    memset(&Command[5], 0, 0x20);
			    Command[0x25] = mempack_crc(&Command[5]);
			 }
		       else
			 {
			    address &= 0xFFE0;
			    if (address <= 0x7FE0)
			      {
				 	 memcpy(&Command[5], &mempack[Control][address], 0x20);
			      }
			    else
			      {
				 memset(&Command[5], 0, 0x20);
			      }
			    Command[0x25] = mempack_crc(&Command[5]);
			 }
		    }
		  break;
		case PLUGIN_RAW:
		  if (controllerCommand != NULL) controllerCommand(Control, Command);
		  break;
		default:
		  memset(&Command[5], 0, 0x20);
		  Command[0x25] = 0;
	       }
	  }
	else
	  Command[1] = 0xA1;
	break;
      case 0x03: // write controller pack
	if (Controls[Control].Present)
	  {
	     Command[1] = 0x01;
	     switch(Controls[Control].Plugin)
	       {
		case PLUGIN_MEMPAK:
		    {
		       int address = (Command[3] << 8) | Command[4];
		       if (address == 0x8001)
			 Command[0x25] = mempack_crc(&Command[5]);
		       else
			 {
			    address &= 0xFFE0;
			    if (address <= 0x7FE0)
			      {
	                 mempakWritten = TRUE;
					 memcpy(&mempack[Control][address], &Command[5], 0x20);
				 	 Command[0x25] = mempack_crc(&Command[5]);
			 }
		    }
		  break;
		case PLUGIN_RAW:
		  if (controllerCommand != NULL) controllerCommand(Control, Command);
		  break;
		default:
		  Command[0x25] = mempack_crc(&Command[5]);
	       }
       }
	  }
	else
	  Command[1] = 0x81;
	break;
     }
}

void update_pif_write()
{
   int i=0, channel=0;
#ifdef DEBUG_PIF
//   printf("write\n");
   print_pif();
#endif
   if (PIF_RAMb[0x3F] > 1)
     {
	switch (PIF_RAMb[0x3F])
	  {
	   case 0x02:
	     for (i=0; i<sizeof(pif2_lut)/32; i++)
	       {
		  if (!memcmp(PIF_RAMb + 64-2*8, pif2_lut[i][0], 16))
		    {
		       memcpy(PIF_RAMb + 64-2*8, pif2_lut[i][1], 16);
		       return;
		    }
	       }
/*	     printf("unknown pif2 code:\n");
	     for (i=(64-2*8)/8; i<(64/8); i++)
	       printf("%x %x %x %x | %x %x %x %x\n",
		      PIF_RAMb[i*8+0], PIF_RAMb[i*8+1],PIF_RAMb[i*8+2], PIF_RAMb[i*8+3],
		      PIF_RAMb[i*8+4], PIF_RAMb[i*8+5],PIF_RAMb[i*8+6], PIF_RAMb[i*8+7]);
*/	     break;
	   case 0x08:
	     PIF_RAMb[0x3F] = 0;
	     break;
/*	   default:
	   break;
//	     printf("error in update_pif_write : %x\n", PIF_RAMb[0x3F]);
*/	  }
	return;
     }
   while (i<0x40)
     {
	switch(PIF_RAMb[i])
	  {
	   case 0x00:
	     channel++;
	     if (channel > 6) i=0x40;
	     break;
	   case 0xFF:
	     break;
	   default:
	     if (!(PIF_RAMb[i] & 0xC0))
	       {
		  {
		     static int wdump_cnt = 0;
		     static u32 wdump_skip = 0;
		     if (wdump_cnt < 6 || (wdump_cnt < 60 && (wdump_skip++ % 100) == 0))
		       {
			  wdump_cnt++;
			  printf("[PIFWRITE] %02X %02X %02X %02X %02X %02X %02X %02X | %02X %02X %02X %02X %02X %02X %02X %02X\n",
				 PIF_RAMb[0],PIF_RAMb[1],PIF_RAMb[2],PIF_RAMb[3],
				 PIF_RAMb[4],PIF_RAMb[5],PIF_RAMb[6],PIF_RAMb[7],
				 PIF_RAMb[8],PIF_RAMb[9],PIF_RAMb[10],PIF_RAMb[11],
				 PIF_RAMb[12],PIF_RAMb[13],PIF_RAMb[14],PIF_RAMb[15]);
		       }
		  }
		  if (channel < 4)
		    {
		       if (Controls[channel].Present &&
			   Controls[channel].RawData)
			 controllerCommand(channel, &PIF_RAMb[i]);
		       else
			 internal_ControllerCommand(channel, &PIF_RAMb[i]);
		    }
		  else if (channel == 4)
		    EepromCommand(&PIF_RAMb[i]);
	//	  else
	//	    printf("channel >= 4 in update_pif_write\n");
		  i += PIF_RAMb[i] + (PIF_RAMb[(i+1)] & 0x3F) + 1;
		  channel++;
	       }
	     else
	       i=0x40;
	  }
	i++;
     }
   //PIF_RAMb[0x3F] = 0;
   controllerCommand(-1, NULL);
#ifdef DEBUG_PIF
   print_pif();
#endif
}

void update_pif_read()
{
   int i=0, channel=0;
#ifdef DEBUG_PIF
//   printf("read\n");
   print_pif();
#endif
   {
      static int dump_cnt = 0;
      static u32 dump_skip = 0;
      if (dump_cnt < 10 || (dump_cnt < 60 && (dump_skip++ % 100) == 0))
	{
	   dump_cnt++;
	   printf("[PIFREAD] pc=%08X %02X %02X %02X %02X %02X %02X %02X %02X | %02X %02X %02X %02X %02X %02X %02X %02X\n",
		  (unsigned int)r4300.pc,
		  PIF_RAMb[0],PIF_RAMb[1],PIF_RAMb[2],PIF_RAMb[3],
		  PIF_RAMb[4],PIF_RAMb[5],PIF_RAMb[6],PIF_RAMb[7],
		  PIF_RAMb[8],PIF_RAMb[9],PIF_RAMb[10],PIF_RAMb[11],
		  PIF_RAMb[12],PIF_RAMb[13],PIF_RAMb[14],PIF_RAMb[15]);
	}
   }
   {
      static const u32 vats[4] = { 0x803289CC, 0x803274FC, 0x8032857C, 0x80317088 };
      static int done[4] = {0,0,0,0};
      int vi, k;
      u32 pc = (unsigned int)r4300.pc;
      for (vi = 0; vi < 4; vi++)
	{
	   if (pc == vats[vi] && !done[vi])
	     {
		u32 va = vats[vi];
		u32 pa;
		done[vi] = 1;
		if (va >= 0x80000000 && va < 0xC0000000)
		  pa = va & 0x1FFFFFFF;
		else
		  pa = va;
		printf("[CODEDUMP va=%08X pa=%08X ra=%08X sp=%08X a0=%08X a1=%08X v0=%08X v1=%08X]\n",
		       va, pa,
		       (u32)r4300.gpr[31], (u32)r4300.gpr[29],
		       (u32)r4300.gpr[4], (u32)r4300.gpr[5],
		       (u32)r4300.gpr[2], (u32)r4300.gpr[3]);
		if (rdramb)
		  {
		     for (k = -12; k < 40; k++)
		       {
			  u32 a = pa + k*4;
			  printf("   %c%03X: %02X %02X %02X %02X\n", (k<0)?'-':'+', (k<0)?(-k*4):(k*4),
				 rdramb[a + 0], rdramb[a + 1],
				 rdramb[a + 2], rdramb[a + 3]);
		       }
		  }
		break;
	     }
	}
   }
   while (i<0x40)
     {
	switch(PIF_RAMb[i])
	  {
	   case 0x00:
	     channel++;
	     if (channel > 6) i=0x40;
	     break;
	   case 0xFE:
	     i = 0x40;
	     break;
	   case 0xFF:
	     break;
	   case 0xB4:
	   case 0x56:
	   case 0xB8:
	     break;
	   default:
	     if (!(PIF_RAMb[i] & 0xC0))
	       {
		  if (channel < 4)
		    {
		       if (Controls[channel].Present &&
			   Controls[channel].RawData)
			 readController(channel, &PIF_RAMb[i]);
		       else
			 internal_ReadController(channel, &PIF_RAMb[i]);
		    }
		  i += PIF_RAMb[i] + (PIF_RAMb[(i+1)] & 0x3F) + 1;
		  channel++;
	       }
	     else
	       i=0x40;
	  }
	i++;
     }
   readController(-1, NULL);
#ifdef DEBUG_PIF
   print_pif();
#endif
}
