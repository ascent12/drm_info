#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRM_PATH "/dev/dri"
#define DRM_MAJOR 226
#define DRM_MINOR_MAX 127

static void driver_info(int fd)
{
	drmVersion *ver = drmGetVersion(fd);
	if (!ver) {
		printf("Driver: Failed to get version\n");
		return;
	}

	printf("Driver: %s %d.%d.%d %s\n", ver->name, ver->version_major,
		ver->version_minor, ver->version_patchlevel, ver->date);

	drmFreeVersion(ver);
}

static void drm_info(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return;
	}

	printf("Device: %s\n", path);
	driver_info(fd);

	close(fd);
}

int main(void)
{
	char path[PATH_MAX];
	for (int i = 0;; ++i) {
		snprintf(path, sizeof path, DRM_DEV_NAME, DRM_DIR_NAME, i);
		if (access(path, R_OK) < 0)
			break;

		drm_info(path);
	}
}
