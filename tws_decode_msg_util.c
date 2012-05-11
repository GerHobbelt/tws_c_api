#define TWS_DEBUG
#include "twsapi.h"
#include "twsapi-debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <ctype.h>


#if defined(_MSC_VER)
#pragma warning(disable: 4100)
#endif


typedef struct tws_client_custom_data
{
	FILE *inf;
	FILE *outf;
} tws_client_custom_data_t;



int tws_tx_mode = 0;
int tws_rx_state = 0;
int tws_rx_field_marker = 0;
int quiet_mode = 0;
char fake_hello_response[80];
const char *fake_resp_pos = NULL;

char *strend(char *s)
{
	return s + strlen(s);
}

int tws_transmit_func(void *arg, const void *buf, unsigned int buflen)
{
	/* message is flushed/written to output, fake the server as a very simple state machine */
	tws_tx_mode++;

	return buflen;
}

int tws_receive_func(void *arg, void *buf_, unsigned int max_bufsize)
{
	tws_client_custom_data_t *cd = (tws_client_custom_data_t *)arg;
	char *buf = (char *)buf_;
	unsigned int len = 0;

	switch (tws_tx_mode)
	{
	case 3:
		len = (unsigned int)strlen(fake_resp_pos);
		if (len >= max_bufsize)
		{
			memcpy(buf, fake_resp_pos, max_bufsize);
			fake_resp_pos += max_bufsize;
			len = max_bufsize;
		}
		else
		{
			len++;
			memcpy(buf, fake_resp_pos, len);
			fake_resp_pos += len;
		}
		break;

	case 5:
		/* 
		fetch data from stdin/file

		make it easy on ourselves: fetch one field at at a time...
		*/
		if (cd->inf)
		{
			while (!feof(cd->inf) && len + 1 < max_bufsize)
			{
				int c = fgetc(cd->inf);

				if (c == 0)
				{
					buf[len++] = c;
					break;
				}
				else if (c < 0)
				{
					return feof(cd->inf) ? len : c;
				}

				switch (tws_rx_state)
				{
				case 0: /* new message start? */
					if (c && isspace(c))
						continue; /* skip whitespace */
					switch (c)
					{
					case '#':
						/* skip comment line in input: */
						tws_rx_state = 1;
						continue;

					case '[':
						/* ignore index markers in input: '[n]' */
						tws_rx_state = 2;
						continue;

					case '"':
					case '\'':
						/* 
						field is delimited by quotes:
						*/
						tws_rx_field_marker = c;
						tws_rx_state = 3;
						continue;

					default:
						/* field is not delimited by quotes, but by whitespace/NUL: */
						tws_rx_field_marker = 0;
						tws_rx_state = 4;
						buf[len++] = c;
						continue;
					}
					break;

				case 1:
					/* skip until end of line: */
					if (c != '\n')
						continue;
					tws_rx_field_marker = 0;
					tws_rx_state = 0;
					continue;

				case 2:
					/* ignore index marker '[n]' i.e. skip until beyond ']': */
					if (c != '\n' && c != ']')
						continue;
					tws_rx_field_marker = 0;
					tws_rx_state = 0;
					continue;

				case 3:
					/* in quote delimited field: */
					if (c != tws_rx_field_marker && c != '\\')
					{
						buf[len++] = c;
						continue;
					}
					else if (c == '\\')
					{
						c = fgetc(cd->inf);
						if (c == tws_rx_field_marker)
						{
							/* 'escaped' quote: */
							buf[len++] = c;
							continue;
						}
						else
						{
							buf[len++] = '\\';
							buf[len++] = c;
							continue;
						}
					}
					else
					{
						/* end quote! */
						buf[len++] = 0;
						tws_rx_field_marker = 0;
						tws_rx_state = 0;
						continue;
					}
					break;

				case 4:
					/* in WS/NUL delimited field: */
					if (c == '\n')
					{
						/* end of line/message? */
						buf[len++] = 0;
						tws_rx_field_marker = 0;
						tws_rx_state = 0;
						continue;
					}
					else if (isspace(c))
					{
						/* end of field */
						buf[len++] = 0;
						tws_rx_field_marker = 0;
						tws_rx_state = 0;
						continue;
					}
					else
					{
						buf[len++] = c;
						continue;
					}
					break;
				}
			}
		}
		break;
	}
	return len;
}

/* 'flush()' marks the end of the outgoing message: it should be transmitted ASAP */
int tws_flush_func(void *arg)
{
	time_t now = time(NULL);
	char *dst;

	switch (tws_tx_mode)
	{
	case 2:
		/* open fake TX/RX exchange happening: HELLO going out now... */
		tws_tx_mode++;
		fake_resp_pos = fake_hello_response;
		memset(fake_hello_response, 0, sizeof(fake_hello_response));
		sprintf(fake_hello_response, "%d", MIN_SERVER_VER_TRAILING_PERCENT + 1);
		dst = strend(fake_hello_response) + 1;
		strftime(dst, sizeof(fake_hello_response) /* bogus */, "%Y%m%d %H%M%S UTC", gmtime(&now));
		break;

	case 4:
		/* open fake TX/RX exchange completing: HELLO received; will send client ID next and then we're done, ready for the real stuff... */
		tws_tx_mode++;
		break;

	default:
		break;
	}
	return 0;
}

/* open callback is invoked when tws_connect is invoked and no connection has been established yet (tws_connected() == false); return 0 on success; a twsclient_error_codes error code on failure. */
int tws_open_func(void *arg)
{
	tws_tx_mode = 1;
	return 0;
}

/* close callback is invoked on error or when tws_disconnect is invoked */
int tws_close_func(void *arg)
{
	return 0;
}




#include "callbacks.c"


void tws_cb_printf(void *opaque, int indent_level, const char *msg, ...)
{
	va_list args;
	tws_client_custom_data_t *cd = (tws_client_custom_data_t *)opaque;

	va_start(args, msg);
	vfprintf(cd->outf, msg, args);
	va_end(args);
}

void tws_debug_printf(void *opaque, const char *msg, ...)
{
	va_list args;
	tws_client_custom_data_t *cd = (tws_client_custom_data_t *)opaque;

	va_start(args, msg);
	if (!quiet_mode)
		vfprintf(stderr, msg, args);
	va_end(args);
}





const char *get_fpath_argval(const char *opt_arg, int *opt_idx_ref, int argc, char **argv)
{
	if (opt_arg && *opt_arg)
	{
		return opt_arg;
	}
	else if (*opt_idx_ref + 1 < argc)
	{
		return argv[++(*opt_idx_ref)];
	}
	return NULL;
}

const char *get_appname(const char *argv0)
{
	const char *p = strrchr(argv0, ':');
	if (p) argv0 = p;
	p = strrchr(argv0, '\\');
	if (p) argv0 = p;
	p = strrchr(argv0, '/');
	if (p) argv0 = p;
	return argv0 + strspn(argv0, ":\\/");
}

int main(int argc, char *argv[])
{
    tws_instance_t *ti;
	tws_client_custom_data_t cd =
	{
		stdin,
		stdout
	};
	int i;
	int rv;

    if(argc < 2) 
	{
usage:
        printf("Usage: %s <options>\n"
			"\n"
			"Decode/print message(s) content in human readable form to stdout.\n"
			"\n"
			"Options:\n"
			"  -d            decode TWS/IB message traffic from stdin.\n"
			"  -i <infile>   load message(s) from file <infile> instead of stdin.\n"
			"  -o <outfile>  write human readable output to file <outfile>\n"
			"                instead of stdout.\n"
			"  -q            do NOT print any 'debug' messages originating from the\n"
			"                TWS API library internals. This is 'quiet mode'...\n"
			"  -h / --help   show this help message.\n"
			"\n"
			, get_appname(argv[0]));
        return EXIT_FAILURE;
    }

	for (i = 1; i < argc; i++)
	{
		const char *a = argv[i];
		if (0 == strncmp("-i", a, 2))
		{
			const char *fpath = get_fpath_argval(a + 2, &i, argc, argv);
			if (cd.inf != stdin)
			{
				fprintf(stderr, "ERROR: You may specify only 1 input file.\n");
				return EXIT_FAILURE;
			}
			cd.inf = fopen(fpath, "rb");
			if (!cd.inf)
			{
				fprintf(stderr, "ERROR: Cannot open input file [%s].\n", fpath);
				return EXIT_FAILURE;
			}
			continue;
		}
		if (0 == strncmp("-o", a, 2))
		{
			const char *fpath = get_fpath_argval(a + 2, &i, argc, argv);
			if (cd.outf != stdout)
			{
				fprintf(stderr, "ERROR: You may specify only 1 output file.\n");
				return EXIT_FAILURE;
			}
			cd.outf = fopen(fpath, "w");
			if (!cd.outf)
			{
				fprintf(stderr, "ERROR: Cannot open output file [%s].\n", fpath);
				return EXIT_FAILURE;
			}
			continue;
		}
		if (0 == strcmp("-h", a) || 0 == strcmp("--help", a) || 0 == strcmp("-?", a))
		{
			goto usage;
		}
		if (0 == strcmp("-d", a))
		{
			/* ignore; it's the 'default' action but you must spec -d if you want to pipe data from stdin to stdout. */
			continue;
		}
		if (0 == strcmp("-q", a))
		{
			quiet_mode = 1;
			continue;
		}
		fprintf(stderr, "ERROR: Unknown input option/argument [%s]\n", a);
		return EXIT_FAILURE;
	}

	ti = tws_create(&cd, tws_transmit_func, tws_receive_func, tws_flush_func, tws_open_func, tws_close_func, 0, 0);
    if (!ti) 
	{
        fprintf(stderr, "ERROR: failed to initialize the TWS API lib.\n"); 
		return EXIT_FAILURE;
    }
	rv = tws_connect(ti, 666 /* client_id */);

	if (!rv)
	{
	    while(!feof(cd.inf) && 0 == tws_event_process(ti));
	}
	tws_destroy(ti);

	if (cd.inf != stdin && cd.inf != NULL) fclose(cd.inf);
	if (cd.outf != stdout && cd.outf != NULL) fclose(cd.outf);

	if (rv)
	{
		const struct twsclient_errmsg *em = tws_strerror(rv);
		if (em && em->err_msg)
		{
			fprintf(stderr, "ERROR: TWS error %d: %s\n", (int)em->err_code, em->err_msg);
		}
	}

    return (rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
