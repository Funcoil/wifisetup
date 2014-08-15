/* Simple program helping with WiFi setup from shell
 * Intended mainly for Raspberry Pi, should be safe as suid program.
 * Author: Martin Habovštiak <martin.habovstiak@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

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

/** Configures unprotected WiFi
 * \param ssid Network name (SSID)
 */
int setupUnprotected(const char *ssid) {
	fprintf(stderr, "Error: unimplemented method\n");
	return 0;
}

/** Configures WEP "protected" WiFi
 * \param ssid Network name (SSID)
 * \param password Network password (key)
 */
int setupWep(const char *ssid, const char *password) {
	fprintf(stderr, "Error: unimplemented method\n");
	return 0;
}

/** Configures WPA/WPA2 protected WiFi
 * \param ssid Network name (SSID)
 * \param password Network password (key)
 */
int setupWpa(const char *ssid, const char *password) {
	if(!password) return 0;
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
			 "psk=\"%s\"",
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

char *getPassword(char *cmdarg) {
	if(cmdarg) return cmdarg;
	static char *password = NULL;
	static size_t passlength = 0;
	getline(&password, &passlength, stdin);
	password[passlength - 1] = 0; // Discard last character (endline)
	return password;
}

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
	}
	return 0;
}
