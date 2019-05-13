#ifndef DRM_INFO_H
#define DRM_INFO_H

struct json_object;

struct json_object *drm_info(void);
void print_drm(struct json_object *obj);

#endif
