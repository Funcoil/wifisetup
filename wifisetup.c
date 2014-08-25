/* Simple program helping with WiFi setup from shell
 * Intended mainly for Raspberry Pi, should be safe as suid program.
 * Author: Martin Habovštiak <martin.habovstiak@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

/** Prints help message to stderr
 * \param prog Program name (0th argument)
 */
void printHelp(const char *prog) {
	fprintf(stderr, "Usage:\n"
			"\t%s -h"			"\t\t\t\tPrint this message\n"
			"\t%s ESSID"			"\t\t\tSetup unprotected network\n"
			"\t%s wep|wpa ESSID [PASSWORD]"	"\tSetup protected network\n"
			"If no password is provided, it wil be read from stdin (must be LF terminated)\n", prog, prog, prog);
}

/** Updates /etc/network/interfaces with given configuration and head file
 * \param fmt Printf-like format string
 * \return 1 on success, 0 on error
 */
int updateInterfaces(const char *fmt, ...) {
	// Open head file (sort of "include file")
	FILE *head = fopen("/etc/network/interfaces.head", "r");
	if(!head) {
		perror("fopen");
		return 0;
	}

	// Open config file
	FILE *cfgfile = fopen("/etc/network/interfaces", "w");
	if(!head) {
		perror("cfgfile");
		return 0;
	}

	// Copy head to config
	char buf[4096];
	size_t len;
	while((len = fread(buf, 1, 4096, head))) {
		if(fwrite(buf, 1, len, cfgfile) < len) {
			perror("fwrite");
			return 0;
		}
	}
	if(ferror(head)) {
		perror("fread");
		return 0;
	}
	fclose(head);
	if(fprintf(cfgfile, "# Auto generated with wifisetup\n"
			 "# Do not modify, unless you know what you are doing!\n"
		  ) < 0) {
		perror("fprintf");
		return 0;
	}
	va_list va;
	va_start(va, fmt);
	if(vfprintf(cfgfile, fmt, va) < 0) {
		perror("fprintf");
		return 0;
	}
	va_end(va);
	if(fprintf(cfgfile,
				"iface default inet dhcp\n"
				"# End of auto generated section\n"
	) < 0) {
		perror("fprintf");
		return 0;
	}

	if(fdatasync(fileno(cfgfile)) < 0) { // Make sure file is written (avoid corruption) but do not give up on error
		perror("fdatasync");
	}
	fclose(cfgfile);

	return 1; 
}

/** Configures unprotected WiFi
 * \param ssid Network name (SSID)
 */
int setupUnprotected(const char *ssid) {
	return updateInterfaces(
			"iface wlan0 inet dhcp\n"
			"wireless-essid %s\n",
			ssid
	);
}

/** Configures WEP "protected" WiFi
 * \param ssid Network name (SSID)
 * \param password Network password (key)
 */
int setupWep(const char *ssid, const char *password) {
	return updateInterfaces(
			"iface wlan0 inet dhcp\n"
			"wireless-essid %s\n"
			"wireless-key %s\n",
			ssid, password
		);
}

/** Configures WPA/WPA2 protected WiFi
 * \param ssid Network name (SSID)
 * \param password Network password (key)
 */
int setupWpa(const char *ssid, const char *password) {
	if(!password) return 0;
	if(!updateInterfaces("iface wlan0 inet manual\nwpa-roam /etc/wpa_supplicant/wpa_supplicant.conf\n")) return 0;
	FILE *cfgfile = fopen("/etc/wpa_supplicant/wpa_supplicant.conf", "w");
	if(!cfgfile) {
		perror("fopen");
		return 0;
	}
	if(fprintf(cfgfile, "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n"
			 "update_config=1\n"
			 "\n"
			 "network={\n"
			 "\tssid=\"%s\"\n"
			 "\tpsk=\"%s\"\n"
			 "}\n",
			 ssid, password
	) < 2) {
		perror("fprintf");
		return 0;
	}
	if(fdatasync(fileno(cfgfile)) < 0) {
		perror("fdatasync");
	}
	if(fclose(cfgfile) < 0) {
		perror("fclose");
	}
	return 1;
}

/** If password was not given as argument it's read from stdin.
 * \return Password for WiFi (Zero terminated string)
 */
char *getPassword(char *cmdarg) {
	if(cmdarg) return cmdarg;
	static char *password = NULL;
	static size_t passlength = 0;
	getline(&password, &passlength, stdin);
	password[passlength - 1] = 0; // Discard last character (endline)
	return password;
}

/** Main function
 * handles argument parsing and runs appropriate functions to setup WiFi
 * \param argc Number of arguments given on command line includig zeroth argument (path to program)
 * \param argv Array of pointers to arguments (as zero terminated strings)
 * \return Zero if operation succeeded, non-zero otherwise
 */
int main(int argc, char **argv) {
	if(argc < 2) {
		printHelp(argv[0]);
		return 1;
	}
	if(!strcmp(argv[1], "-h")) {
		printHelp(argv[0]);
		return 0;
	}
	int success = 0;
	if(argc == 2) { // unprotected
		success = setupUnprotected(argv[1]);
	} else {
		if(!strcmp(argv[1], "wep")) success = setupWep(argv[2], getPassword(argv[3])); else
		if(!strcmp(argv[1], "wpa")) success = setupWpa(argv[2], getPassword(argv[3])); else {
			fprintf(stderr, "Unknown network type \"%s\".\n", argv[1]);
			printHelp(argv[0]);
			return 1;
		}
	}

	if(success) {
		int ret = system("ifdown --force wlan0") < 0;
		if(ret < 0) {
			perror("system");
			return 1;
		}
		if(ret > 0) {
			fprintf(stderr, "ifdown failed\n");
			return ret;
		}
		ret = system("ifup wlan0");
		if(ret < 0) {
			perror("system");
			return 1;
		}
		if(ret > 0) {
			fprintf(stderr, "ifup failed\n");
			return ret;
		}
		sleep(1);
		ret = system("dhclient wlan0");
		if(ret < 0) {
			perror("system");
			return 1;
		}
		if(ret > 0) {
			fprintf(stderr, "dhclient failed\n");
			return ret;
		}
	}
	return 0;
}
