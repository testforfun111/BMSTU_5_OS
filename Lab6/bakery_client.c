/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "bakery.h"


void
bakery_prog_1(char *host)
{
	CLIENT *clnt;
	struct REQUEST  *result_1;
	struct REQUEST  get_number_1_arg;
	int  *result_2;
	struct REQUEST  bakery_service_1_arg;

#ifndef	DEBUG
	clnt = clnt_create (host, BAKERY_PROG, BAKERY_VER, "udp");
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}
#endif	/* DEBUG */

	result_1 = get_number_1(&get_number_1_arg, clnt);
	if (result_1 == (struct REQUEST *) NULL) {
		clnt_perror (clnt, "call failed");
	}
	result_2 = bakery_service_1(&bakery_service_1_arg, clnt);
	if (result_2 == (int *) NULL) {
		clnt_perror (clnt, "call failed");
	}
#ifndef	DEBUG
	clnt_destroy (clnt);
#endif	 /* DEBUG */
}


int
main (int argc, char *argv[])
{
	char *host;

	if (argc < 2) {
		printf ("usage: %s server_host\n", argv[0]);
		exit (1);
	}
	host = argv[1];
	bakery_prog_1 (host);
exit (0);
}
