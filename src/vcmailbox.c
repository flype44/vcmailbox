/******************************************************************************
 * 
 * Program:    vcmailbox.c
 * Purpose:    Send messages to the VideoCore via the RPi mailbox.
 * Author:     Philippe Carpentier
 * Target:     PiStorm/Emu68, AmigaOS 3.x
 * Compiler:   SAS/C Amiga Compiler 6.59
 * Repository: https://github.com/flype44/vcmailbox
 * 
 ******************************************************************************/

#include <dos/dos.h>
#include <exec/exec.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/mailbox.h>

#include "utils.h"
#include "vcmailbox.h"

/******************************************************************************
 * 
 * Defines
 * 
 ******************************************************************************/

#define MAX_WORDS (1024)
#define MAILBOXNAME "mailbox.resource"

#define MSG_ERROR_PARSING "An error occured while parsing this request."

#define TEMPLATE "ASCII/S,SWAP/S,FULL/S,VERBOSE/S,WORDS/F/A"

typedef enum {
	OPT_ASCII,
	OPT_SWAP,
	OPT_FULL,
	OPT_VERBOSE,
	OPT_WORDS,
	OPT_COUNT
} OPT_ARGS;

/******************************************************************************
 * 
 * Globals
 * 
 ******************************************************************************/

APTR MailboxBase;

static STRPTR VERSTAG = VERSTRING;

/******************************************************************************
 * 
 * ShowUsage()
 * 
 ******************************************************************************/

static VOID ShowUsage(VOID) {
	PutStr(VERSTAG + 6);
	PutStr("\n\n");
	PutStr("Usage: vcmailbox [options] [word0] [word1] ...\n\n");
	PutStr("Send messages to the VideoCore via the RPi mailbox.\n");
	PutStr("Returns 0 when completed successfully.\n");
	PutStr("For further documentation please see:\n");
	PutStr("https://github.com/flype44/vcmailbox\n\n");
	PutStr("Examples:\n");
	PutStr("> vcmailbox 0x10002 4 0 0\n");
	PutStr("> vcmailbox SWAP 0x10003 8 0 0 0\n");
	PutStr("> vcmailbox ASCII 0x50001 512\n");
	PutStr("> vcmailbox FULL 0x10002 4 0 0\n");
	PutStr("> vcmailbox VERBOSE 0x10002 4 0 0\n");
	PutStr("> vcmailbox VERBOSE FULL 0x10002 4 0 0\n");
	PutStr("> vcmailbox VERBOSE FULL 0x30004 8 0 3 0\n");
	PutStr("> vcmailbox VERBOSE FULL 0x30047 8 0 3 0 0x30004 8 0 3 0\n\n");
}

/******************************************************************************
 * 
 * ParseULong() 
 * 
 ******************************************************************************/

static BOOL ParseULong(CONST_STRPTR s, ULONG * out)
{
	ULONG value = 0, base = 10, digit;
	BOOL seen = FALSE;
	UBYTE c;

	while (*s == ' ' || *s == '\t')
		s++;

	if (*s == '0')
	{
		if (s[1] == 'x' || s[1] == 'X')
		{
			base = 16;
			s += 2;
		}
		else
		{
			base = 8;
			seen = TRUE;
			s++;
		}
	}

	for (;; s++)
	{
		c = (UBYTE)*s;

		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else
			break;

		if (digit >= base)
			return (FALSE);

		if (value > (0xFFFFFFFFUL - digit) / base)
			return (FALSE);

		value = value * base + digit;
		seen = TRUE;
	}

	if (!seen || *s != '\0')
		return (FALSE);

	*out = value;

	return (TRUE);
}

/******************************************************************************
 * 
 * vcmailbox()
 * 
 ******************************************************************************/

static LONG vcmailbox(ULONG argc, STRPTR * argv, LONG * opts)
{
	ULONG i, j, k, n, v, p, size, total;
	BOOL ok;

	// Too big for the stack (default shell stack is about 4 KB)
	static ULONG words[MAX_WORDS];

	// Clear request
	for (i = 0; i < MAX_WORDS; i++) {
		words[i] = 0;
	}

	// Set request body
	for (i = 1; i < argc; i++) {
		if (!ParseULong(argv[i], &words[i + 1])) {
			Printf("Invalid number: %s\n", argv[i]);
			return (RETURN_ERROR);
		}
	}

	// Compute the request length from the declared tag buffer sizes,
	// so the trailing zero words of the last tag may be omitted
	p = 2;
	while (p < argc + 1) {
		size = (p + 1 < argc + 1) ? words[p + 1] : 0;
		if (size > (MAX_WORDS << 2)) {
			Printf("Tag buffer too large (max: %lu bytes) !\n", MAX_WORDS << 2);
			return (RETURN_ERROR);
		}
		p += 3 + ((size + 3) >> 2);
	}
	total = p + 1;
	if (total > MAX_WORDS) {
		Printf("Too much words (max: %lu words) !\n", MAX_WORDS);
		return (RETURN_ERROR);
	}

	// Set request size
	words[0] = total << 2;

	// Send request
	MB_RawCommand(words);

	// Keep the request status
	ok = (words[1] & 0x80000000) ? TRUE : FALSE;

	// Show request size
	if (opts[OPT_FULL]) {
		if (opts[OPT_VERBOSE]) {
			Printf("[%02lu] 0x%08lx ; Size (%lu bytes)\n", 
				0, words[0], words[0]);
		} else {
			Printf("0x%08lx ", words[0]);
		}
	}
	
	// Show request code
	if (opts[OPT_FULL]) {
		if (opts[OPT_VERBOSE]) {
			Printf("[%02lu] 0x%08lx ; Code (%s)\n", 
				1, words[1], (words[1] & 0x7fffffff) ? 
				"Error" : "Success");
		} else {
			Printf("0x%08lx ", words[1]);
		}
	}
	
	// Check request code
	if (words[1] & 0x7fffffff) {
		PutStr("\n");
		PutStr(MSG_ERROR_PARSING);
		PutStr("\n");
		PutStr("Partial response, require more WORDS.\n");
		return (RETURN_WARN);
	}

	// Show request body
	i = 2;
	j = 0;
	while (i < total && (i + 3) <= MAX_WORDS)
	{
		// Show request tag identifier
		if (opts[OPT_FULL]) {
			if (opts[OPT_VERBOSE]) {
				Printf("[%02lu] 0x%08lx ; Tag[%lu] Identifier\n", 
					i, words[i], j);
			} else {
				Printf("0x%08lx ", words[i]);
			}
		}
		i++;
		
		// Show request tag size
		if (opts[OPT_FULL]) {
			if (opts[OPT_VERBOSE]) {
				Printf("[%02lu] 0x%08lx ; Tag[%lu] Size (%lu bytes)\n", 
					i, words[i], j, words[i]);
			} else {
				Printf("0x%08lx ", words[i]);
			}
		}
		i++;
		
		// Show request tag code
		n = (words[i] & 0x7fffffff);
		if (opts[OPT_FULL]) {
			if (opts[OPT_VERBOSE]) {
				Printf("[%02lu] 0x%08lx ; Tag[%lu] Code (%lu bytes)\n", 
					i, words[i], j, n);
			} else {
				Printf("0x%08lx ", words[i]);
			}
		}
		
		// Check request tag code
		if (n > words[i - 1]) {
			PutStr("\n");
			PutStr(MSG_ERROR_PARSING);
			PutStr("\n");
			Printf("Required response length for this request is %lu bytes.\n", n);
			return (RETURN_WARN);
		}
		i++;

		// Count request tag responses
		if (n % 4) n += (4 - (n % 4));
		n >>= 2;

		// Check response fits in the message buffer
		if ((i + n) >= MAX_WORDS) {
			PutStr("\n");
			PutStr(MSG_ERROR_PARSING);
			PutStr("\n");
			PutStr("Response exceeds the message buffer.\n");
			return (RETURN_WARN);
		}

		// Show request tag response as ASCII
		if (opts[OPT_ASCII]) {
			// Undo the byteswapping done by mailbox.resource
			for (k = 0; k < n; k++) {
				words[i + k] = LE32(words[i + k]);
			}
			Printf("%s\n", &words[i]);
			break;
		}

		// Show request tag responses
		for (k = 0; k < n; k++) {
			// Undo the byteswapping done by mailbox.resource
			v = opts[OPT_SWAP] ? LE32(words[i]) : words[i];
			if (opts[OPT_VERBOSE]) {
				Printf("[%02lu] 0x%08lx ; Tag[%lu] Response[%02lu] => %lu\n",
					i, v, j, k, v);
			} else {
				Printf("0x%08lx ", v);
			}
			i++;
		}
		
		// Check request end
		if (words[i] == 0) {
			if (opts[OPT_FULL]) {
				if (opts[OPT_VERBOSE]) {
					Printf("[%02lu] 0x%08lx ; End\n", i, 0);
				} else {
					Printf("0x%08lx ", 0);
				}
			}
			break;
		}
		
		j++;
	}
	
	// String termination
	if (!opts[OPT_ASCII] && !opts[OPT_VERBOSE]) {
		PutStr("\n");
	}
	
	// Check request code
	return (ok ? RETURN_OK : RETURN_WARN);
}

/******************************************************************************
 * 
 * main()
 * 
 ******************************************************************************/

ULONG main(ULONG argc, STRPTR * argv)
{
	struct RDArgs * rdargs;
	
	LONG opts[OPT_COUNT];
	opts[OPT_ASCII  ] = 0L;
	opts[OPT_SWAP   ] = 0L;
	opts[OPT_FULL   ] = 0L;
	opts[OPT_VERBOSE] = 0L;
	opts[OPT_WORDS  ] = 0L;
	
	// Show usage
	if (argc < 2) {
		ShowUsage();
		return (RETURN_OK);
	}
	
	// Open mailbox.resource
	if (!(MailboxBase = OpenResource(MAILBOXNAME))) {
		PutStr("Cant open " MAILBOXNAME " !\n");
		return (RETURN_FAIL);
	}
	
	// Check arguments
	if ((argc + 1) >= MAX_WORDS) {
		Printf("Too much words (max: %lu words) !\n", MAX_WORDS);
		return (RETURN_ERROR);
	}
	
	// Read arguments
	if (!(rdargs = (struct RDArgs *)ReadArgs(TEMPLATE, opts, NULL))) {
		ShowUsage();
		return (RETURN_ERROR);
	}
	
	if (opts[OPT_ASCII]) {
		argc--;
		argv++;
	}

	if (opts[OPT_SWAP]) {
		argc--;
		argv++;
	}
	
	if (opts[OPT_FULL]) {
		argc--;
		argv++;
	}
	
	if (opts[OPT_VERBOSE]) {
		argc--;
		argv++;
	}
	
	FreeArgs(rdargs);
	
	// Execute command
	return (vcmailbox(argc, argv, opts));
}

/******************************************************************************
 * 
 * End of file
 * 
 ******************************************************************************/
