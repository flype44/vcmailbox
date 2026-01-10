/******************************************************************************
 * Program:  vcmailbox.c
 * Purpose:  Send a command to the VideoCore and print the result.
 * Authors:  Philippe CARPENTIER
 * Target:   AmigaOS 3.x
 * Compiler: SAS/C Amiga Compiler 6.59
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <exec/exec.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/devicetree.h>
#include <proto/mailbox.h>

#include "utils.h"
#include "vcmailbox.h"

/******************************************************************************
 * Defines
 ******************************************************************************/

#define MAILBOXNAME "mailbox.resource"
#define DEVICETREENAME "devicetree.resource"
#define GETASCII(c) ((UBYTE)((c > 31 && isascii(c)) ? c : 32))

/******************************************************************************
 * Globals
 ******************************************************************************/

APTR  MailboxBase = NULL;
APTR  DeviceTreeBase = NULL;

static STRPTR VERSTAG = VERSTRING;

extern struct ExecBase * SysBase;
extern struct DosLibrary * DosBase;

/******************************************************************************
 * ShowUsage()
 ******************************************************************************/

static VOID ShowUsage(VOID) {
	PutStr("Usage: vcmailbox [word0] [word1] ...\n");
	PutStr("Send a command to the VideoCore and print the result.\n");
	PutStr("Without any argument this information is shown.\n");
	PutStr("Exit status 0 means command completed successfully ");
	PutStr("else VideoCore return an error.\n");
	PutStr("Examples:\n");
	PutStr("> vcmailbox 0x10002 4 0 0                                   ; board revision\n");
	PutStr("> vcmailbox 0x10003 8 0 0 0                                 ; mac address\n");
	PutStr("> vcmailbox 0x10004 8 0 0 0                                 ; serial number\n");
	PutStr("> vcmailbox 0x10003 8 0 0 0 0x10004 8 0 0 0                 ; mac address and serial number\n");
	PutStr("> vcmailbox 0x30003 8 0 1 0                                 ; voltage\n");
	PutStr("> vcmailbox 0x30006 8 0 0 0 0x3000a 8 0 0 0                 ; temperature and temperaturemax\n");
	PutStr("> vcmailbox 0x30009 8 0 0 0                                 ; turbo\n");
	PutStr("> vcmailbox 0x30047 8 0 3 0 0x30007 8 0 3 0 0x30004 8 0 3 0 ; clock rates\n");
}

/******************************************************************************
 * 
 * Display()
 * 
 ******************************************************************************/

VOID Display(ULONG * msg, ULONG size)
{
	ULONG i = 0;
	ULONG j = 0;
	ULONG k = 0;
	ULONG value;
	
	value = msg[i];
	printf("[%02lu] %08lx ; Request Size (%lu bytes)\n", i, value, value);
	i++;
	
	value = msg[i];
	printf("[%02lu] %08lx ; Request Code (%s)\n", i, value, value == 0x80000000 ? "Success" : "Error");
	i++;
	
	while (i < size)
	{
		ULONG ResponseSize;
		
		value = msg[i];
		printf("[%02lu] %08x ; Tag[%02lu] Identifier\n", i, value, j);
		i++;
		
		value = msg[i];
		printf("[%02lu] %08x ; Tag[%02lu] Size (%lu bytes)\n", i, value, j, value);
		i++;
		
		value = msg[i];
		ResponseSize = (value & 0x7fffffff);
		printf("[%02lu] %08x ; Tag[%02lu] Code (%s, %lu bytes)\n", i, value, j, (value & 0x80000000) ? "Success" : "Error", ResponseSize);
		i++;
		
		if (ResponseSize % 4)
			ResponseSize += (4 - (ResponseSize % 4));
		
		ResponseSize >>= 2;
		
		for (k = 0; k < ResponseSize; k++)
		{
			value = msg[i];
			
			printf("[%02lu] %08x ; Tag[%02lu] Response[%02lu] => %lu\n", 
				i, value, j, k, value);
			
			i++;
		}
		
		if (msg[i] == 0)
		{
			printf("[%02lu] %08x ; Request end\n", i, 0);
			break;
		}
		
		j++;
	}
}

/******************************************************************************
 * 
 * main()
 * 
 ******************************************************************************/

#define MAX_WORDS (1024)
#define MAX_BYTES (MAX_WORDS << 2)

ULONG main(ULONG argc, UBYTE * argv[])
{
	ULONG i;
	ULONG FBReq[MAX_WORDS];
	
	// Open devicetree.resource
	if (!(DeviceTreeBase = OpenResource(DEVICETREENAME))) {
		PutStr("Cant open " DEVICETREENAME "\n");
		return (RETURN_FAIL);
	}
	
	// Open mailbox.resource
	if (!(MailboxBase = OpenResource(MAILBOXNAME))) {
		PutStr("Cant open " MAILBOXNAME "\n");
		return (RETURN_FAIL);
	}
	
	// Check arguments
	if (argc < 2 || argc + 1 >= MAX_WORDS) {
		ShowUsage();
		return (RETURN_ERROR);
	}
	
	// Prepare request
	FBReq[0] = (argc + 2) * 4;
	FBReq[1] = 0;
	for (i = 1; i < argc; i++) {
		FBReq[i + 1] = strtoul(argv[i], 0, 0);
	}
	FBReq[argc + 0] = 0;
	FBReq[argc + 1] = 0;
	FBReq[argc + 2] = 0;
	
	// Send request
	MB_RawCommand(FBReq);
	
	// Display reply
	Display(FBReq, argc + 2);
	return (RETURN_OK);
}

/******************************************************************************
 * End of file
 ******************************************************************************/
