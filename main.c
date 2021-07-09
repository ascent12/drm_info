#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <json_object.h>
#include <json_util.h>

#include "drm_info.h"

int main(int argc, char *argv[])
{
	bool json = false, input = false;

	int opt;
	while ((opt = getopt(argc, argv, "ji")) != -1) {
		switch (opt) {
		case 'j':
			json = true;
			break;
		case 'i':
			input = true;
			break;
		default:
			fprintf(stderr, "usage: drm_info [-j] [-i] [--] [path]...\n");
			exit(opt == '?' ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	}

	struct json_object *obj;
	if (input) {
		obj = json_object_from_fd(STDIN_FILENO);
	} else {
		obj = drm_info(&argv[optind]);
	}
	if (!obj) {
		exit(EXIT_FAILURE);
	}

	if (json) {
		json_object_to_fd(STDOUT_FILENO, obj,
			JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
	} else {
		print_drm(obj);
	}
	json_object_put(obj);
	return EXIT_SUCCESS;
}
