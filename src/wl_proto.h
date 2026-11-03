// made by protocols/gen.py from wayland-scanner output, dont edit
/*
wayland:
Copyright © 2008-2011 Kristian Høgsberg
Copyright © 2010-2011 Intel Corporation
Copyright © 2012-2013 Collabora, Ltd.

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

wlr_layer_shell_unstable_v1:
Copyright © 2017 Drew DeVault

Permission to use, copy, modify, distribute, and sell this
software and its documentation for any purpose is hereby granted
without fee, provided that the above copyright notice appear in
all copies and that both that copyright notice and this permission
notice appear in supporting documentation, and that the name of
the copyright holders not be used in advertising or publicity
pertaining to distribution of the software without specific,
written prior permission.  The copyright holders make no
representations about the suitability of this software for any
purpose.  It is provided "as is" without express or implied
warranty.

THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

wlr_screencopy_unstable_v1:
Copyright © 2018 Simon Ser
Copyright © 2019 Andri Yngvason

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

xdg_output_unstable_v1:
Copyright © 2017 Red Hat Inc.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
#ifndef GM_WL_PROTO_H
#define GM_WL_PROTO_H

#if defined(__GNUC__) && __GNUC__ >= 4
#define GM_WL_PRIVATE __attribute__((visibility("hidden")))
#else
#define GM_WL_PRIVATE
#endif

struct wl_buffer;
struct wl_callback;
struct wl_compositor;
struct wl_display;
struct wl_keyboard;
struct wl_output;
struct wl_pointer;
struct wl_region;
struct wl_registry;
struct wl_seat;
struct wl_shm;
struct wl_shm_pool;
struct wl_surface;
struct wl_touch;

#ifndef WL_DISPLAY_INTERFACE
#define WL_DISPLAY_INTERFACE

extern const struct wl_interface gm_wl_display_interface;
#endif
#ifndef WL_REGISTRY_INTERFACE
#define WL_REGISTRY_INTERFACE

extern const struct wl_interface gm_wl_registry_interface;
#endif
#ifndef WL_CALLBACK_INTERFACE
#define WL_CALLBACK_INTERFACE

extern const struct wl_interface gm_wl_callback_interface;
#endif
#ifndef WL_COMPOSITOR_INTERFACE
#define WL_COMPOSITOR_INTERFACE

extern const struct wl_interface gm_wl_compositor_interface;
#endif
#ifndef WL_SHM_POOL_INTERFACE
#define WL_SHM_POOL_INTERFACE

extern const struct wl_interface gm_wl_shm_pool_interface;
#endif
#ifndef WL_SHM_INTERFACE
#define WL_SHM_INTERFACE

extern const struct wl_interface gm_wl_shm_interface;
#endif
#ifndef WL_BUFFER_INTERFACE
#define WL_BUFFER_INTERFACE

extern const struct wl_interface gm_wl_buffer_interface;
#endif
#ifndef WL_SURFACE_INTERFACE
#define WL_SURFACE_INTERFACE

extern const struct wl_interface gm_wl_surface_interface;
#endif
#ifndef WL_SEAT_INTERFACE
#define WL_SEAT_INTERFACE

extern const struct wl_interface gm_wl_seat_interface;
#endif
#ifndef WL_POINTER_INTERFACE
#define WL_POINTER_INTERFACE

extern const struct wl_interface gm_wl_pointer_interface;
#endif
#ifndef WL_KEYBOARD_INTERFACE
#define WL_KEYBOARD_INTERFACE

extern const struct wl_interface gm_wl_keyboard_interface;
#endif
#ifndef WL_TOUCH_INTERFACE
#define WL_TOUCH_INTERFACE

extern const struct wl_interface gm_wl_touch_interface;
#endif
#ifndef WL_OUTPUT_INTERFACE
#define WL_OUTPUT_INTERFACE

extern const struct wl_interface gm_wl_output_interface;
#endif
#ifndef WL_REGION_INTERFACE
#define WL_REGION_INTERFACE

extern const struct wl_interface gm_wl_region_interface;
#endif

#ifndef WL_DISPLAY_ERROR_ENUM
#define WL_DISPLAY_ERROR_ENUM

enum wl_display_error {

	WL_DISPLAY_ERROR_INVALID_OBJECT = 0,

	WL_DISPLAY_ERROR_INVALID_METHOD = 1,

	WL_DISPLAY_ERROR_NO_MEMORY = 2,

	WL_DISPLAY_ERROR_IMPLEMENTATION = 3,
};
#endif

struct wl_display_listener {

	void (*error)(void *data,
		      struct wl_display *wl_display,
		      void *object_id,
		      uint32_t code,
		      const char *message);

	void (*delete_id)(void *data,
			  struct wl_display *wl_display,
			  uint32_t id);
};

static inline int
wl_display_add_listener(struct wl_display *wl_display,
			const struct wl_display_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_display,
				     (void (**)(void)) listener, data);
}

#define WL_DISPLAY_SYNC 0
#define WL_DISPLAY_GET_REGISTRY 1

#define WL_DISPLAY_ERROR_SINCE_VERSION 1

#define WL_DISPLAY_DELETE_ID_SINCE_VERSION 1

#define WL_DISPLAY_SYNC_SINCE_VERSION 1

#define WL_DISPLAY_GET_REGISTRY_SINCE_VERSION 1

static inline void
wl_display_set_user_data(struct wl_display *wl_display, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_display, user_data);
}

static inline void *
wl_display_get_user_data(struct wl_display *wl_display)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_display);
}

static inline uint32_t
wl_display_get_version(struct wl_display *wl_display)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_display);
}

static inline struct wl_callback *
wl_display_sync(struct wl_display *wl_display)
{
	struct wl_proxy *callback;

	callback = wl_proxy_marshal_flags((struct wl_proxy *) wl_display,
			 WL_DISPLAY_SYNC, &gm_wl_callback_interface, wl_proxy_get_version((struct wl_proxy *) wl_display), 0, NULL);

	return (struct wl_callback *) callback;
}

static inline struct wl_registry *
wl_display_get_registry(struct wl_display *wl_display)
{
	struct wl_proxy *registry;

	registry = wl_proxy_marshal_flags((struct wl_proxy *) wl_display,
			 WL_DISPLAY_GET_REGISTRY, &gm_wl_registry_interface, wl_proxy_get_version((struct wl_proxy *) wl_display), 0, NULL);

	return (struct wl_registry *) registry;
}

struct wl_registry_listener {

	void (*global)(void *data,
		       struct wl_registry *wl_registry,
		       uint32_t name,
		       const char *interface,
		       uint32_t version);

	void (*global_remove)(void *data,
			      struct wl_registry *wl_registry,
			      uint32_t name);
};

static inline int
wl_registry_add_listener(struct wl_registry *wl_registry,
			 const struct wl_registry_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_registry,
				     (void (**)(void)) listener, data);
}

#define WL_REGISTRY_BIND 0

#define WL_REGISTRY_GLOBAL_SINCE_VERSION 1

#define WL_REGISTRY_GLOBAL_REMOVE_SINCE_VERSION 1

#define WL_REGISTRY_BIND_SINCE_VERSION 1

static inline void
wl_registry_set_user_data(struct wl_registry *wl_registry, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_registry, user_data);
}

static inline void *
wl_registry_get_user_data(struct wl_registry *wl_registry)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_registry);
}

static inline uint32_t
wl_registry_get_version(struct wl_registry *wl_registry)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_registry);
}

static inline void
wl_registry_destroy(struct wl_registry *wl_registry)
{
	wl_proxy_destroy((struct wl_proxy *) wl_registry);
}

static inline void *
wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_registry,
			 WL_REGISTRY_BIND, interface, version, 0, name, interface->name, version, NULL);

	return (void *) id;
}

struct wl_callback_listener {

	void (*done)(void *data,
		     struct wl_callback *wl_callback,
		     uint32_t callback_data);
};

static inline int
wl_callback_add_listener(struct wl_callback *wl_callback,
			 const struct wl_callback_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_callback,
				     (void (**)(void)) listener, data);
}

#define WL_CALLBACK_DONE_SINCE_VERSION 1

static inline void
wl_callback_set_user_data(struct wl_callback *wl_callback, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_callback, user_data);
}

static inline void *
wl_callback_get_user_data(struct wl_callback *wl_callback)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_callback);
}

static inline uint32_t
wl_callback_get_version(struct wl_callback *wl_callback)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_callback);
}

static inline void
wl_callback_destroy(struct wl_callback *wl_callback)
{
	wl_proxy_destroy((struct wl_proxy *) wl_callback);
}

#define WL_COMPOSITOR_CREATE_SURFACE 0
#define WL_COMPOSITOR_CREATE_REGION 1

#define WL_COMPOSITOR_CREATE_SURFACE_SINCE_VERSION 1

#define WL_COMPOSITOR_CREATE_REGION_SINCE_VERSION 1

static inline void
wl_compositor_set_user_data(struct wl_compositor *wl_compositor, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_compositor, user_data);
}

static inline void *
wl_compositor_get_user_data(struct wl_compositor *wl_compositor)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_compositor);
}

static inline uint32_t
wl_compositor_get_version(struct wl_compositor *wl_compositor)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_compositor);
}

static inline void
wl_compositor_destroy(struct wl_compositor *wl_compositor)
{
	wl_proxy_destroy((struct wl_proxy *) wl_compositor);
}

static inline struct wl_surface *
wl_compositor_create_surface(struct wl_compositor *wl_compositor)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_compositor,
			 WL_COMPOSITOR_CREATE_SURFACE, &gm_wl_surface_interface, wl_proxy_get_version((struct wl_proxy *) wl_compositor), 0, NULL);

	return (struct wl_surface *) id;
}

static inline struct wl_region *
wl_compositor_create_region(struct wl_compositor *wl_compositor)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_compositor,
			 WL_COMPOSITOR_CREATE_REGION, &gm_wl_region_interface, wl_proxy_get_version((struct wl_proxy *) wl_compositor), 0, NULL);

	return (struct wl_region *) id;
}

#define WL_SHM_POOL_CREATE_BUFFER 0
#define WL_SHM_POOL_DESTROY 1
#define WL_SHM_POOL_RESIZE 2

#define WL_SHM_POOL_CREATE_BUFFER_SINCE_VERSION 1

#define WL_SHM_POOL_DESTROY_SINCE_VERSION 1

#define WL_SHM_POOL_RESIZE_SINCE_VERSION 1

static inline void
wl_shm_pool_set_user_data(struct wl_shm_pool *wl_shm_pool, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_shm_pool, user_data);
}

static inline void *
wl_shm_pool_get_user_data(struct wl_shm_pool *wl_shm_pool)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_shm_pool);
}

static inline uint32_t
wl_shm_pool_get_version(struct wl_shm_pool *wl_shm_pool)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_shm_pool);
}

static inline struct wl_buffer *
wl_shm_pool_create_buffer(struct wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_shm_pool,
			 WL_SHM_POOL_CREATE_BUFFER, &gm_wl_buffer_interface, wl_proxy_get_version((struct wl_proxy *) wl_shm_pool), 0, NULL, offset, width, height, stride, format);

	return (struct wl_buffer *) id;
}

static inline void
wl_shm_pool_destroy(struct wl_shm_pool *wl_shm_pool)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_shm_pool,
			 WL_SHM_POOL_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) wl_shm_pool), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
wl_shm_pool_resize(struct wl_shm_pool *wl_shm_pool, int32_t size)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_shm_pool,
			 WL_SHM_POOL_RESIZE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_shm_pool), 0, size);
}

#ifndef WL_SHM_ERROR_ENUM
#define WL_SHM_ERROR_ENUM

enum wl_shm_error {

	WL_SHM_ERROR_INVALID_FORMAT = 0,

	WL_SHM_ERROR_INVALID_STRIDE = 1,

	WL_SHM_ERROR_INVALID_FD = 2,
};
#endif

#ifndef WL_SHM_FORMAT_ENUM
#define WL_SHM_FORMAT_ENUM

enum wl_shm_format {

	WL_SHM_FORMAT_ARGB8888 = 0,

	WL_SHM_FORMAT_XRGB8888 = 1,

	WL_SHM_FORMAT_C8 = 0x20203843,

	WL_SHM_FORMAT_RGB332 = 0x38424752,

	WL_SHM_FORMAT_BGR233 = 0x38524742,

	WL_SHM_FORMAT_XRGB4444 = 0x32315258,

	WL_SHM_FORMAT_XBGR4444 = 0x32314258,

	WL_SHM_FORMAT_RGBX4444 = 0x32315852,

	WL_SHM_FORMAT_BGRX4444 = 0x32315842,

	WL_SHM_FORMAT_ARGB4444 = 0x32315241,

	WL_SHM_FORMAT_ABGR4444 = 0x32314241,

	WL_SHM_FORMAT_RGBA4444 = 0x32314152,

	WL_SHM_FORMAT_BGRA4444 = 0x32314142,

	WL_SHM_FORMAT_XRGB1555 = 0x35315258,

	WL_SHM_FORMAT_XBGR1555 = 0x35314258,

	WL_SHM_FORMAT_RGBX5551 = 0x35315852,

	WL_SHM_FORMAT_BGRX5551 = 0x35315842,

	WL_SHM_FORMAT_ARGB1555 = 0x35315241,

	WL_SHM_FORMAT_ABGR1555 = 0x35314241,

	WL_SHM_FORMAT_RGBA5551 = 0x35314152,

	WL_SHM_FORMAT_BGRA5551 = 0x35314142,

	WL_SHM_FORMAT_RGB565 = 0x36314752,

	WL_SHM_FORMAT_BGR565 = 0x36314742,

	WL_SHM_FORMAT_RGB888 = 0x34324752,

	WL_SHM_FORMAT_BGR888 = 0x34324742,

	WL_SHM_FORMAT_XBGR8888 = 0x34324258,

	WL_SHM_FORMAT_RGBX8888 = 0x34325852,

	WL_SHM_FORMAT_BGRX8888 = 0x34325842,

	WL_SHM_FORMAT_ABGR8888 = 0x34324241,

	WL_SHM_FORMAT_RGBA8888 = 0x34324152,

	WL_SHM_FORMAT_BGRA8888 = 0x34324142,

	WL_SHM_FORMAT_XRGB2101010 = 0x30335258,

	WL_SHM_FORMAT_XBGR2101010 = 0x30334258,

	WL_SHM_FORMAT_RGBX1010102 = 0x30335852,

	WL_SHM_FORMAT_BGRX1010102 = 0x30335842,

	WL_SHM_FORMAT_ARGB2101010 = 0x30335241,

	WL_SHM_FORMAT_ABGR2101010 = 0x30334241,

	WL_SHM_FORMAT_RGBA1010102 = 0x30334152,

	WL_SHM_FORMAT_BGRA1010102 = 0x30334142,

	WL_SHM_FORMAT_YUYV = 0x56595559,

	WL_SHM_FORMAT_YVYU = 0x55595659,

	WL_SHM_FORMAT_UYVY = 0x59565955,

	WL_SHM_FORMAT_VYUY = 0x59555956,

	WL_SHM_FORMAT_AYUV = 0x56555941,

	WL_SHM_FORMAT_NV12 = 0x3231564e,

	WL_SHM_FORMAT_NV21 = 0x3132564e,

	WL_SHM_FORMAT_NV16 = 0x3631564e,

	WL_SHM_FORMAT_NV61 = 0x3136564e,

	WL_SHM_FORMAT_YUV410 = 0x39565559,

	WL_SHM_FORMAT_YVU410 = 0x39555659,

	WL_SHM_FORMAT_YUV411 = 0x31315559,

	WL_SHM_FORMAT_YVU411 = 0x31315659,

	WL_SHM_FORMAT_YUV420 = 0x32315559,

	WL_SHM_FORMAT_YVU420 = 0x32315659,

	WL_SHM_FORMAT_YUV422 = 0x36315559,

	WL_SHM_FORMAT_YVU422 = 0x36315659,

	WL_SHM_FORMAT_YUV444 = 0x34325559,

	WL_SHM_FORMAT_YVU444 = 0x34325659,

	WL_SHM_FORMAT_R8 = 0x20203852,

	WL_SHM_FORMAT_R16 = 0x20363152,

	WL_SHM_FORMAT_RG88 = 0x38384752,

	WL_SHM_FORMAT_GR88 = 0x38385247,

	WL_SHM_FORMAT_RG1616 = 0x32334752,

	WL_SHM_FORMAT_GR1616 = 0x32335247,

	WL_SHM_FORMAT_XRGB16161616F = 0x48345258,

	WL_SHM_FORMAT_XBGR16161616F = 0x48344258,

	WL_SHM_FORMAT_ARGB16161616F = 0x48345241,

	WL_SHM_FORMAT_ABGR16161616F = 0x48344241,

	WL_SHM_FORMAT_XYUV8888 = 0x56555958,

	WL_SHM_FORMAT_VUY888 = 0x34325556,

	WL_SHM_FORMAT_VUY101010 = 0x30335556,

	WL_SHM_FORMAT_Y210 = 0x30313259,

	WL_SHM_FORMAT_Y212 = 0x32313259,

	WL_SHM_FORMAT_Y216 = 0x36313259,

	WL_SHM_FORMAT_Y410 = 0x30313459,

	WL_SHM_FORMAT_Y412 = 0x32313459,

	WL_SHM_FORMAT_Y416 = 0x36313459,

	WL_SHM_FORMAT_XVYU2101010 = 0x30335658,

	WL_SHM_FORMAT_XVYU12_16161616 = 0x36335658,

	WL_SHM_FORMAT_XVYU16161616 = 0x38345658,

	WL_SHM_FORMAT_Y0L0 = 0x304c3059,

	WL_SHM_FORMAT_X0L0 = 0x304c3058,

	WL_SHM_FORMAT_Y0L2 = 0x324c3059,

	WL_SHM_FORMAT_X0L2 = 0x324c3058,
	WL_SHM_FORMAT_YUV420_8BIT = 0x38305559,
	WL_SHM_FORMAT_YUV420_10BIT = 0x30315559,
	WL_SHM_FORMAT_XRGB8888_A8 = 0x38415258,
	WL_SHM_FORMAT_XBGR8888_A8 = 0x38414258,
	WL_SHM_FORMAT_RGBX8888_A8 = 0x38415852,
	WL_SHM_FORMAT_BGRX8888_A8 = 0x38415842,
	WL_SHM_FORMAT_RGB888_A8 = 0x38413852,
	WL_SHM_FORMAT_BGR888_A8 = 0x38413842,
	WL_SHM_FORMAT_RGB565_A8 = 0x38413552,
	WL_SHM_FORMAT_BGR565_A8 = 0x38413542,

	WL_SHM_FORMAT_NV24 = 0x3432564e,

	WL_SHM_FORMAT_NV42 = 0x3234564e,

	WL_SHM_FORMAT_P210 = 0x30313250,

	WL_SHM_FORMAT_P010 = 0x30313050,

	WL_SHM_FORMAT_P012 = 0x32313050,

	WL_SHM_FORMAT_P016 = 0x36313050,

	WL_SHM_FORMAT_AXBXGXRX106106106106 = 0x30314241,

	WL_SHM_FORMAT_NV15 = 0x3531564e,
	WL_SHM_FORMAT_Q410 = 0x30313451,
	WL_SHM_FORMAT_Q401 = 0x31303451,

	WL_SHM_FORMAT_XRGB16161616 = 0x38345258,

	WL_SHM_FORMAT_XBGR16161616 = 0x38344258,

	WL_SHM_FORMAT_ARGB16161616 = 0x38345241,

	WL_SHM_FORMAT_ABGR16161616 = 0x38344241,

	WL_SHM_FORMAT_C1 = 0x20203143,

	WL_SHM_FORMAT_C2 = 0x20203243,

	WL_SHM_FORMAT_C4 = 0x20203443,

	WL_SHM_FORMAT_D1 = 0x20203144,

	WL_SHM_FORMAT_D2 = 0x20203244,

	WL_SHM_FORMAT_D4 = 0x20203444,

	WL_SHM_FORMAT_D8 = 0x20203844,

	WL_SHM_FORMAT_R1 = 0x20203152,

	WL_SHM_FORMAT_R2 = 0x20203252,

	WL_SHM_FORMAT_R4 = 0x20203452,

	WL_SHM_FORMAT_R10 = 0x20303152,

	WL_SHM_FORMAT_R12 = 0x20323152,

	WL_SHM_FORMAT_AVUY8888 = 0x59555641,

	WL_SHM_FORMAT_XVUY8888 = 0x59555658,

	WL_SHM_FORMAT_P030 = 0x30333050,
};
#endif

struct wl_shm_listener {

	void (*format)(void *data,
		       struct wl_shm *wl_shm,
		       uint32_t format);
};

static inline int
wl_shm_add_listener(struct wl_shm *wl_shm,
		    const struct wl_shm_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_shm,
				     (void (**)(void)) listener, data);
}

#define WL_SHM_CREATE_POOL 0
#define WL_SHM_RELEASE 1

#define WL_SHM_FORMAT_SINCE_VERSION 1

#define WL_SHM_CREATE_POOL_SINCE_VERSION 1

#define WL_SHM_RELEASE_SINCE_VERSION 2

static inline void
wl_shm_set_user_data(struct wl_shm *wl_shm, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_shm, user_data);
}

static inline void *
wl_shm_get_user_data(struct wl_shm *wl_shm)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_shm);
}

static inline uint32_t
wl_shm_get_version(struct wl_shm *wl_shm)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_shm);
}

static inline void
wl_shm_destroy(struct wl_shm *wl_shm)
{
	wl_proxy_destroy((struct wl_proxy *) wl_shm);
}

static inline struct wl_shm_pool *
wl_shm_create_pool(struct wl_shm *wl_shm, int32_t fd, int32_t size)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_shm,
			 WL_SHM_CREATE_POOL, &gm_wl_shm_pool_interface, wl_proxy_get_version((struct wl_proxy *) wl_shm), 0, NULL, fd, size);

	return (struct wl_shm_pool *) id;
}

static inline void
wl_shm_release(struct wl_shm *wl_shm)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_shm,
			 WL_SHM_RELEASE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_shm), WL_MARSHAL_FLAG_DESTROY);
}

struct wl_buffer_listener {

	void (*release)(void *data,
			struct wl_buffer *wl_buffer);
};

static inline int
wl_buffer_add_listener(struct wl_buffer *wl_buffer,
		       const struct wl_buffer_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_buffer,
				     (void (**)(void)) listener, data);
}

#define WL_BUFFER_DESTROY 0

#define WL_BUFFER_RELEASE_SINCE_VERSION 1

#define WL_BUFFER_DESTROY_SINCE_VERSION 1

static inline void
wl_buffer_set_user_data(struct wl_buffer *wl_buffer, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_buffer, user_data);
}

static inline void *
wl_buffer_get_user_data(struct wl_buffer *wl_buffer)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_buffer);
}

static inline uint32_t
wl_buffer_get_version(struct wl_buffer *wl_buffer)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_buffer);
}

static inline void
wl_buffer_destroy(struct wl_buffer *wl_buffer)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_buffer,
			 WL_BUFFER_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) wl_buffer), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef WL_SURFACE_ERROR_ENUM
#define WL_SURFACE_ERROR_ENUM

enum wl_surface_error {

	WL_SURFACE_ERROR_INVALID_SCALE = 0,

	WL_SURFACE_ERROR_INVALID_TRANSFORM = 1,

	WL_SURFACE_ERROR_INVALID_SIZE = 2,

	WL_SURFACE_ERROR_INVALID_OFFSET = 3,

	WL_SURFACE_ERROR_DEFUNCT_ROLE_OBJECT = 4,
};
#endif

struct wl_surface_listener {

	void (*enter)(void *data,
		      struct wl_surface *wl_surface,
		      struct wl_output *output);

	void (*leave)(void *data,
		      struct wl_surface *wl_surface,
		      struct wl_output *output);

	void (*preferred_buffer_scale)(void *data,
				       struct wl_surface *wl_surface,
				       int32_t factor);

	void (*preferred_buffer_transform)(void *data,
					   struct wl_surface *wl_surface,
					   uint32_t transform);
};

static inline int
wl_surface_add_listener(struct wl_surface *wl_surface,
			const struct wl_surface_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_surface,
				     (void (**)(void)) listener, data);
}

#define WL_SURFACE_DESTROY 0
#define WL_SURFACE_ATTACH 1
#define WL_SURFACE_DAMAGE 2
#define WL_SURFACE_FRAME 3
#define WL_SURFACE_SET_OPAQUE_REGION 4
#define WL_SURFACE_SET_INPUT_REGION 5
#define WL_SURFACE_COMMIT 6
#define WL_SURFACE_SET_BUFFER_TRANSFORM 7
#define WL_SURFACE_SET_BUFFER_SCALE 8
#define WL_SURFACE_DAMAGE_BUFFER 9
#define WL_SURFACE_OFFSET 10

#define WL_SURFACE_ENTER_SINCE_VERSION 1

#define WL_SURFACE_LEAVE_SINCE_VERSION 1

#define WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION 6

#define WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION 6

#define WL_SURFACE_DESTROY_SINCE_VERSION 1

#define WL_SURFACE_ATTACH_SINCE_VERSION 1

#define WL_SURFACE_DAMAGE_SINCE_VERSION 1

#define WL_SURFACE_FRAME_SINCE_VERSION 1

#define WL_SURFACE_SET_OPAQUE_REGION_SINCE_VERSION 1

#define WL_SURFACE_SET_INPUT_REGION_SINCE_VERSION 1

#define WL_SURFACE_COMMIT_SINCE_VERSION 1

#define WL_SURFACE_SET_BUFFER_TRANSFORM_SINCE_VERSION 2

#define WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION 3

#define WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION 4

#define WL_SURFACE_OFFSET_SINCE_VERSION 5

static inline void
wl_surface_set_user_data(struct wl_surface *wl_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_surface, user_data);
}

static inline void *
wl_surface_get_user_data(struct wl_surface *wl_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_surface);
}

static inline uint32_t
wl_surface_get_version(struct wl_surface *wl_surface)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_surface);
}

static inline void
wl_surface_destroy(struct wl_surface *wl_surface)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_ATTACH, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, buffer, x, y);
}

static inline void
wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_DAMAGE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, x, y, width, height);
}

static inline struct wl_callback *
wl_surface_frame(struct wl_surface *wl_surface)
{
	struct wl_proxy *callback;

	callback = wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_FRAME, &gm_wl_callback_interface, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, NULL);

	return (struct wl_callback *) callback;
}

static inline void
wl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_OPAQUE_REGION, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, region);
}

static inline void
wl_surface_set_input_region(struct wl_surface *wl_surface, struct wl_region *region)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_INPUT_REGION, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, region);
}

static inline void
wl_surface_commit(struct wl_surface *wl_surface)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_COMMIT, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0);
}

static inline void
wl_surface_set_buffer_transform(struct wl_surface *wl_surface, int32_t transform)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_BUFFER_TRANSFORM, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, transform);
}

static inline void
wl_surface_set_buffer_scale(struct wl_surface *wl_surface, int32_t scale)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_SET_BUFFER_SCALE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, scale);
}

static inline void
wl_surface_damage_buffer(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_DAMAGE_BUFFER, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, x, y, width, height);
}

static inline void
wl_surface_offset(struct wl_surface *wl_surface, int32_t x, int32_t y)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_surface,
			 WL_SURFACE_OFFSET, NULL, wl_proxy_get_version((struct wl_proxy *) wl_surface), 0, x, y);
}

#ifndef WL_SEAT_CAPABILITY_ENUM
#define WL_SEAT_CAPABILITY_ENUM

enum wl_seat_capability {

	WL_SEAT_CAPABILITY_POINTER = 1,

	WL_SEAT_CAPABILITY_KEYBOARD = 2,

	WL_SEAT_CAPABILITY_TOUCH = 4,
};
#endif

#ifndef WL_SEAT_ERROR_ENUM
#define WL_SEAT_ERROR_ENUM

enum wl_seat_error {

	WL_SEAT_ERROR_MISSING_CAPABILITY = 0,
};
#endif

struct wl_seat_listener {

	void (*capabilities)(void *data,
			     struct wl_seat *wl_seat,
			     uint32_t capabilities);

	void (*name)(void *data,
		     struct wl_seat *wl_seat,
		     const char *name);
};

static inline int
wl_seat_add_listener(struct wl_seat *wl_seat,
		     const struct wl_seat_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_seat,
				     (void (**)(void)) listener, data);
}

#define WL_SEAT_GET_POINTER 0
#define WL_SEAT_GET_KEYBOARD 1
#define WL_SEAT_GET_TOUCH 2
#define WL_SEAT_RELEASE 3

#define WL_SEAT_CAPABILITIES_SINCE_VERSION 1

#define WL_SEAT_NAME_SINCE_VERSION 2

#define WL_SEAT_GET_POINTER_SINCE_VERSION 1

#define WL_SEAT_GET_KEYBOARD_SINCE_VERSION 1

#define WL_SEAT_GET_TOUCH_SINCE_VERSION 1

#define WL_SEAT_RELEASE_SINCE_VERSION 5

static inline void
wl_seat_set_user_data(struct wl_seat *wl_seat, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_seat, user_data);
}

static inline void *
wl_seat_get_user_data(struct wl_seat *wl_seat)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_seat);
}

static inline uint32_t
wl_seat_get_version(struct wl_seat *wl_seat)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_seat);
}

static inline void
wl_seat_destroy(struct wl_seat *wl_seat)
{
	wl_proxy_destroy((struct wl_proxy *) wl_seat);
}

static inline struct wl_pointer *
wl_seat_get_pointer(struct wl_seat *wl_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_seat,
			 WL_SEAT_GET_POINTER, &gm_wl_pointer_interface, wl_proxy_get_version((struct wl_proxy *) wl_seat), 0, NULL);

	return (struct wl_pointer *) id;
}

static inline struct wl_keyboard *
wl_seat_get_keyboard(struct wl_seat *wl_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_seat,
			 WL_SEAT_GET_KEYBOARD, &gm_wl_keyboard_interface, wl_proxy_get_version((struct wl_proxy *) wl_seat), 0, NULL);

	return (struct wl_keyboard *) id;
}

static inline struct wl_touch *
wl_seat_get_touch(struct wl_seat *wl_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) wl_seat,
			 WL_SEAT_GET_TOUCH, &gm_wl_touch_interface, wl_proxy_get_version((struct wl_proxy *) wl_seat), 0, NULL);

	return (struct wl_touch *) id;
}

static inline void
wl_seat_release(struct wl_seat *wl_seat)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_seat,
			 WL_SEAT_RELEASE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_seat), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef WL_POINTER_ERROR_ENUM
#define WL_POINTER_ERROR_ENUM
enum wl_pointer_error {

	WL_POINTER_ERROR_ROLE = 0,
};
#endif

#ifndef WL_POINTER_BUTTON_STATE_ENUM
#define WL_POINTER_BUTTON_STATE_ENUM

enum wl_pointer_button_state {

	WL_POINTER_BUTTON_STATE_RELEASED = 0,

	WL_POINTER_BUTTON_STATE_PRESSED = 1,
};
#endif

#ifndef WL_POINTER_AXIS_ENUM
#define WL_POINTER_AXIS_ENUM

enum wl_pointer_axis {

	WL_POINTER_AXIS_VERTICAL_SCROLL = 0,

	WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1,
};
#endif

#ifndef WL_POINTER_AXIS_SOURCE_ENUM
#define WL_POINTER_AXIS_SOURCE_ENUM

enum wl_pointer_axis_source {

	WL_POINTER_AXIS_SOURCE_WHEEL = 0,

	WL_POINTER_AXIS_SOURCE_FINGER = 1,

	WL_POINTER_AXIS_SOURCE_CONTINUOUS = 2,

	WL_POINTER_AXIS_SOURCE_WHEEL_TILT = 3,
};

#define WL_POINTER_AXIS_SOURCE_WHEEL_TILT_SINCE_VERSION 6
#endif

#ifndef WL_POINTER_AXIS_RELATIVE_DIRECTION_ENUM
#define WL_POINTER_AXIS_RELATIVE_DIRECTION_ENUM

enum wl_pointer_axis_relative_direction {

	WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL = 0,

	WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED = 1,
};
#endif

struct wl_pointer_listener {

	void (*enter)(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      wl_fixed_t surface_x,
		      wl_fixed_t surface_y);

	void (*leave)(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      struct wl_surface *surface);

	void (*motion)(void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t time,
		       wl_fixed_t surface_x,
		       wl_fixed_t surface_y);

	void (*button)(void *data,
		       struct wl_pointer *wl_pointer,
		       uint32_t serial,
		       uint32_t time,
		       uint32_t button,
		       uint32_t state);

	void (*axis)(void *data,
		     struct wl_pointer *wl_pointer,
		     uint32_t time,
		     uint32_t axis,
		     wl_fixed_t value);

	void (*frame)(void *data,
		      struct wl_pointer *wl_pointer);

	void (*axis_source)(void *data,
			    struct wl_pointer *wl_pointer,
			    uint32_t axis_source);

	void (*axis_stop)(void *data,
			  struct wl_pointer *wl_pointer,
			  uint32_t time,
			  uint32_t axis);

	void (*axis_discrete)(void *data,
			      struct wl_pointer *wl_pointer,
			      uint32_t axis,
			      int32_t discrete);

	void (*axis_value120)(void *data,
			      struct wl_pointer *wl_pointer,
			      uint32_t axis,
			      int32_t value120);

	void (*axis_relative_direction)(void *data,
					struct wl_pointer *wl_pointer,
					uint32_t axis,
					uint32_t direction);
};

static inline int
wl_pointer_add_listener(struct wl_pointer *wl_pointer,
			const struct wl_pointer_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_pointer,
				     (void (**)(void)) listener, data);
}

#define WL_POINTER_SET_CURSOR 0
#define WL_POINTER_RELEASE 1

#define WL_POINTER_ENTER_SINCE_VERSION 1

#define WL_POINTER_LEAVE_SINCE_VERSION 1

#define WL_POINTER_MOTION_SINCE_VERSION 1

#define WL_POINTER_BUTTON_SINCE_VERSION 1

#define WL_POINTER_AXIS_SINCE_VERSION 1

#define WL_POINTER_FRAME_SINCE_VERSION 5

#define WL_POINTER_AXIS_SOURCE_SINCE_VERSION 5

#define WL_POINTER_AXIS_STOP_SINCE_VERSION 5

#define WL_POINTER_AXIS_DISCRETE_SINCE_VERSION 5

#define WL_POINTER_AXIS_VALUE120_SINCE_VERSION 8

#define WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION 9

#define WL_POINTER_SET_CURSOR_SINCE_VERSION 1

#define WL_POINTER_RELEASE_SINCE_VERSION 3

static inline void
wl_pointer_set_user_data(struct wl_pointer *wl_pointer, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_pointer, user_data);
}

static inline void *
wl_pointer_get_user_data(struct wl_pointer *wl_pointer)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_pointer);
}

static inline uint32_t
wl_pointer_get_version(struct wl_pointer *wl_pointer)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_pointer);
}

static inline void
wl_pointer_destroy(struct wl_pointer *wl_pointer)
{
	wl_proxy_destroy((struct wl_proxy *) wl_pointer);
}

static inline void
wl_pointer_set_cursor(struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, int32_t hotspot_x, int32_t hotspot_y)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_pointer,
			 WL_POINTER_SET_CURSOR, NULL, wl_proxy_get_version((struct wl_proxy *) wl_pointer), 0, serial, surface, hotspot_x, hotspot_y);
}

static inline void
wl_pointer_release(struct wl_pointer *wl_pointer)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_pointer,
			 WL_POINTER_RELEASE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_pointer), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef WL_KEYBOARD_KEYMAP_FORMAT_ENUM
#define WL_KEYBOARD_KEYMAP_FORMAT_ENUM

enum wl_keyboard_keymap_format {

	WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP = 0,

	WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 = 1,
};
#endif

#ifndef WL_KEYBOARD_KEY_STATE_ENUM
#define WL_KEYBOARD_KEY_STATE_ENUM

enum wl_keyboard_key_state {

	WL_KEYBOARD_KEY_STATE_RELEASED = 0,

	WL_KEYBOARD_KEY_STATE_PRESSED = 1,

	WL_KEYBOARD_KEY_STATE_REPEATED = 2,
};

#define WL_KEYBOARD_KEY_STATE_REPEATED_SINCE_VERSION 10
#endif

struct wl_keyboard_listener {

	void (*keymap)(void *data,
		       struct wl_keyboard *wl_keyboard,
		       uint32_t format,
		       int32_t fd,
		       uint32_t size);

	void (*enter)(void *data,
		      struct wl_keyboard *wl_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface,
		      struct wl_array *keys);

	void (*leave)(void *data,
		      struct wl_keyboard *wl_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface);

	void (*key)(void *data,
		    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t key,
		    uint32_t state);

	void (*modifiers)(void *data,
			  struct wl_keyboard *wl_keyboard,
			  uint32_t serial,
			  uint32_t mods_depressed,
			  uint32_t mods_latched,
			  uint32_t mods_locked,
			  uint32_t group);

	void (*repeat_info)(void *data,
			    struct wl_keyboard *wl_keyboard,
			    int32_t rate,
			    int32_t delay);
};

static inline int
wl_keyboard_add_listener(struct wl_keyboard *wl_keyboard,
			 const struct wl_keyboard_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_keyboard,
				     (void (**)(void)) listener, data);
}

#define WL_KEYBOARD_RELEASE 0

#define WL_KEYBOARD_KEYMAP_SINCE_VERSION 1

#define WL_KEYBOARD_ENTER_SINCE_VERSION 1

#define WL_KEYBOARD_LEAVE_SINCE_VERSION 1

#define WL_KEYBOARD_KEY_SINCE_VERSION 1

#define WL_KEYBOARD_MODIFIERS_SINCE_VERSION 1

#define WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION 4

#define WL_KEYBOARD_RELEASE_SINCE_VERSION 3

static inline void
wl_keyboard_set_user_data(struct wl_keyboard *wl_keyboard, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_keyboard, user_data);
}

static inline void *
wl_keyboard_get_user_data(struct wl_keyboard *wl_keyboard)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_keyboard);
}

static inline uint32_t
wl_keyboard_get_version(struct wl_keyboard *wl_keyboard)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_keyboard);
}

static inline void
wl_keyboard_destroy(struct wl_keyboard *wl_keyboard)
{
	wl_proxy_destroy((struct wl_proxy *) wl_keyboard);
}

static inline void
wl_keyboard_release(struct wl_keyboard *wl_keyboard)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_keyboard,
			 WL_KEYBOARD_RELEASE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_keyboard), WL_MARSHAL_FLAG_DESTROY);
}

struct wl_touch_listener {

	void (*down)(void *data,
		     struct wl_touch *wl_touch,
		     uint32_t serial,
		     uint32_t time,
		     struct wl_surface *surface,
		     int32_t id,
		     wl_fixed_t x,
		     wl_fixed_t y);

	void (*up)(void *data,
		   struct wl_touch *wl_touch,
		   uint32_t serial,
		   uint32_t time,
		   int32_t id);

	void (*motion)(void *data,
		       struct wl_touch *wl_touch,
		       uint32_t time,
		       int32_t id,
		       wl_fixed_t x,
		       wl_fixed_t y);

	void (*frame)(void *data,
		      struct wl_touch *wl_touch);

	void (*cancel)(void *data,
		       struct wl_touch *wl_touch);

	void (*shape)(void *data,
		      struct wl_touch *wl_touch,
		      int32_t id,
		      wl_fixed_t major,
		      wl_fixed_t minor);

	void (*orientation)(void *data,
			    struct wl_touch *wl_touch,
			    int32_t id,
			    wl_fixed_t orientation);
};

static inline int
wl_touch_add_listener(struct wl_touch *wl_touch,
		      const struct wl_touch_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_touch,
				     (void (**)(void)) listener, data);
}

#define WL_TOUCH_RELEASE 0

#define WL_TOUCH_DOWN_SINCE_VERSION 1

#define WL_TOUCH_UP_SINCE_VERSION 1

#define WL_TOUCH_MOTION_SINCE_VERSION 1

#define WL_TOUCH_FRAME_SINCE_VERSION 1

#define WL_TOUCH_CANCEL_SINCE_VERSION 1

#define WL_TOUCH_SHAPE_SINCE_VERSION 6

#define WL_TOUCH_ORIENTATION_SINCE_VERSION 6

#define WL_TOUCH_RELEASE_SINCE_VERSION 3

static inline void
wl_touch_set_user_data(struct wl_touch *wl_touch, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_touch, user_data);
}

static inline void *
wl_touch_get_user_data(struct wl_touch *wl_touch)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_touch);
}

static inline uint32_t
wl_touch_get_version(struct wl_touch *wl_touch)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_touch);
}

static inline void
wl_touch_destroy(struct wl_touch *wl_touch)
{
	wl_proxy_destroy((struct wl_proxy *) wl_touch);
}

static inline void
wl_touch_release(struct wl_touch *wl_touch)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_touch,
			 WL_TOUCH_RELEASE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_touch), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef WL_OUTPUT_SUBPIXEL_ENUM
#define WL_OUTPUT_SUBPIXEL_ENUM

enum wl_output_subpixel {

	WL_OUTPUT_SUBPIXEL_UNKNOWN = 0,

	WL_OUTPUT_SUBPIXEL_NONE = 1,

	WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB = 2,

	WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR = 3,

	WL_OUTPUT_SUBPIXEL_VERTICAL_RGB = 4,

	WL_OUTPUT_SUBPIXEL_VERTICAL_BGR = 5,
};
#endif

#ifndef WL_OUTPUT_TRANSFORM_ENUM
#define WL_OUTPUT_TRANSFORM_ENUM

enum wl_output_transform {

	WL_OUTPUT_TRANSFORM_NORMAL = 0,

	WL_OUTPUT_TRANSFORM_90 = 1,

	WL_OUTPUT_TRANSFORM_180 = 2,

	WL_OUTPUT_TRANSFORM_270 = 3,

	WL_OUTPUT_TRANSFORM_FLIPPED = 4,

	WL_OUTPUT_TRANSFORM_FLIPPED_90 = 5,

	WL_OUTPUT_TRANSFORM_FLIPPED_180 = 6,

	WL_OUTPUT_TRANSFORM_FLIPPED_270 = 7,
};
#endif

#ifndef WL_OUTPUT_MODE_ENUM
#define WL_OUTPUT_MODE_ENUM

enum wl_output_mode {

	WL_OUTPUT_MODE_CURRENT = 0x1,

	WL_OUTPUT_MODE_PREFERRED = 0x2,
};
#endif

struct wl_output_listener {

	void (*geometry)(void *data,
			 struct wl_output *wl_output,
			 int32_t x,
			 int32_t y,
			 int32_t physical_width,
			 int32_t physical_height,
			 int32_t subpixel,
			 const char *make,
			 const char *model,
			 int32_t transform);

	void (*mode)(void *data,
		     struct wl_output *wl_output,
		     uint32_t flags,
		     int32_t width,
		     int32_t height,
		     int32_t refresh);

	void (*done)(void *data,
		     struct wl_output *wl_output);

	void (*scale)(void *data,
		      struct wl_output *wl_output,
		      int32_t factor);

	void (*name)(void *data,
		     struct wl_output *wl_output,
		     const char *name);

	void (*description)(void *data,
			    struct wl_output *wl_output,
			    const char *description);
};

static inline int
wl_output_add_listener(struct wl_output *wl_output,
		       const struct wl_output_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) wl_output,
				     (void (**)(void)) listener, data);
}

#define WL_OUTPUT_RELEASE 0

#define WL_OUTPUT_GEOMETRY_SINCE_VERSION 1

#define WL_OUTPUT_MODE_SINCE_VERSION 1

#define WL_OUTPUT_DONE_SINCE_VERSION 2

#define WL_OUTPUT_SCALE_SINCE_VERSION 2

#define WL_OUTPUT_NAME_SINCE_VERSION 4

#define WL_OUTPUT_DESCRIPTION_SINCE_VERSION 4

#define WL_OUTPUT_RELEASE_SINCE_VERSION 3

static inline void
wl_output_set_user_data(struct wl_output *wl_output, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_output, user_data);
}

static inline void *
wl_output_get_user_data(struct wl_output *wl_output)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_output);
}

static inline uint32_t
wl_output_get_version(struct wl_output *wl_output)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_output);
}

static inline void
wl_output_destroy(struct wl_output *wl_output)
{
	wl_proxy_destroy((struct wl_proxy *) wl_output);
}

static inline void
wl_output_release(struct wl_output *wl_output)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_output,
			 WL_OUTPUT_RELEASE, NULL, wl_proxy_get_version((struct wl_proxy *) wl_output), WL_MARSHAL_FLAG_DESTROY);
}

#define WL_REGION_DESTROY 0
#define WL_REGION_ADD 1
#define WL_REGION_SUBTRACT 2

#define WL_REGION_DESTROY_SINCE_VERSION 1

#define WL_REGION_ADD_SINCE_VERSION 1

#define WL_REGION_SUBTRACT_SINCE_VERSION 1

static inline void
wl_region_set_user_data(struct wl_region *wl_region, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) wl_region, user_data);
}

static inline void *
wl_region_get_user_data(struct wl_region *wl_region)
{
	return wl_proxy_get_user_data((struct wl_proxy *) wl_region);
}

static inline uint32_t
wl_region_get_version(struct wl_region *wl_region)
{
	return wl_proxy_get_version((struct wl_proxy *) wl_region);
}

static inline void
wl_region_destroy(struct wl_region *wl_region)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_region,
			 WL_REGION_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) wl_region), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_region,
			 WL_REGION_ADD, NULL, wl_proxy_get_version((struct wl_proxy *) wl_region), 0, x, y, width, height);
}

static inline void
wl_region_subtract(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal_flags((struct wl_proxy *) wl_region,
			 WL_REGION_SUBTRACT, NULL, wl_proxy_get_version((struct wl_proxy *) wl_region), 0, x, y, width, height);
}

struct wl_output;
struct wl_surface;
struct xdg_popup;
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

#ifndef ZWLR_LAYER_SHELL_V1_INTERFACE
#define ZWLR_LAYER_SHELL_V1_INTERFACE

extern const struct wl_interface gm_zwlr_layer_shell_v1_interface;
#endif
#ifndef ZWLR_LAYER_SURFACE_V1_INTERFACE
#define ZWLR_LAYER_SURFACE_V1_INTERFACE

extern const struct wl_interface gm_zwlr_layer_surface_v1_interface;
#endif

#ifndef ZWLR_LAYER_SHELL_V1_ERROR_ENUM
#define ZWLR_LAYER_SHELL_V1_ERROR_ENUM
enum zwlr_layer_shell_v1_error {

	ZWLR_LAYER_SHELL_V1_ERROR_ROLE = 0,

	ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER = 1,

	ZWLR_LAYER_SHELL_V1_ERROR_ALREADY_CONSTRUCTED = 2,
};
#endif

#ifndef ZWLR_LAYER_SHELL_V1_LAYER_ENUM
#define ZWLR_LAYER_SHELL_V1_LAYER_ENUM

enum zwlr_layer_shell_v1_layer {
	ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND = 0,
	ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM = 1,
	ZWLR_LAYER_SHELL_V1_LAYER_TOP = 2,
	ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY = 3,
};
#endif

#define ZWLR_LAYER_SHELL_V1_GET_LAYER_SURFACE 0
#define ZWLR_LAYER_SHELL_V1_DESTROY 1

#define ZWLR_LAYER_SHELL_V1_GET_LAYER_SURFACE_SINCE_VERSION 1

#define ZWLR_LAYER_SHELL_V1_DESTROY_SINCE_VERSION 3

static inline void
zwlr_layer_shell_v1_set_user_data(struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwlr_layer_shell_v1, user_data);
}

static inline void *
zwlr_layer_shell_v1_get_user_data(struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwlr_layer_shell_v1);
}

static inline uint32_t
zwlr_layer_shell_v1_get_version(struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwlr_layer_shell_v1);
}

static inline struct zwlr_layer_surface_v1 *
zwlr_layer_shell_v1_get_layer_surface(struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1, struct wl_surface *surface, struct wl_output *output, uint32_t layer, const char *namespace)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_shell_v1,
			 ZWLR_LAYER_SHELL_V1_GET_LAYER_SURFACE, &gm_zwlr_layer_surface_v1_interface, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_shell_v1), 0, NULL, surface, output, layer, namespace);

	return (struct zwlr_layer_surface_v1 *) id;
}

static inline void
zwlr_layer_shell_v1_destroy(struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_shell_v1,
			 ZWLR_LAYER_SHELL_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_shell_v1), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ENUM
#define ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ENUM

enum zwlr_layer_surface_v1_keyboard_interactivity {

	ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE = 0,

	ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE = 1,

	ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND = 2,
};

#define ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND_SINCE_VERSION 4
#endif

#ifndef ZWLR_LAYER_SURFACE_V1_ERROR_ENUM
#define ZWLR_LAYER_SURFACE_V1_ERROR_ENUM
enum zwlr_layer_surface_v1_error {

	ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE = 0,

	ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE = 1,

	ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_ANCHOR = 2,

	ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_KEYBOARD_INTERACTIVITY = 3,

	ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_EXCLUSIVE_EDGE = 4,
};
#endif

#ifndef ZWLR_LAYER_SURFACE_V1_ANCHOR_ENUM
#define ZWLR_LAYER_SURFACE_V1_ANCHOR_ENUM
enum zwlr_layer_surface_v1_anchor {

	ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP = 1,

	ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM = 2,

	ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT = 4,

	ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT = 8,
};
#endif

struct zwlr_layer_surface_v1_listener {

	void (*configure)(void *data,
			  struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
			  uint32_t serial,
			  uint32_t width,
			  uint32_t height);

	void (*closed)(void *data,
		       struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1);
};

static inline int
zwlr_layer_surface_v1_add_listener(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
				   const struct zwlr_layer_surface_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwlr_layer_surface_v1,
				     (void (**)(void)) listener, data);
}

#define ZWLR_LAYER_SURFACE_V1_SET_SIZE 0
#define ZWLR_LAYER_SURFACE_V1_SET_ANCHOR 1
#define ZWLR_LAYER_SURFACE_V1_SET_EXCLUSIVE_ZONE 2
#define ZWLR_LAYER_SURFACE_V1_SET_MARGIN 3
#define ZWLR_LAYER_SURFACE_V1_SET_KEYBOARD_INTERACTIVITY 4
#define ZWLR_LAYER_SURFACE_V1_GET_POPUP 5
#define ZWLR_LAYER_SURFACE_V1_ACK_CONFIGURE 6
#define ZWLR_LAYER_SURFACE_V1_DESTROY 7
#define ZWLR_LAYER_SURFACE_V1_SET_LAYER 8
#define ZWLR_LAYER_SURFACE_V1_SET_EXCLUSIVE_EDGE 9

#define ZWLR_LAYER_SURFACE_V1_CONFIGURE_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_CLOSED_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_SET_SIZE_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_SET_ANCHOR_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_SET_EXCLUSIVE_ZONE_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_SET_MARGIN_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_SET_KEYBOARD_INTERACTIVITY_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_GET_POPUP_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_ACK_CONFIGURE_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_DESTROY_SINCE_VERSION 1

#define ZWLR_LAYER_SURFACE_V1_SET_LAYER_SINCE_VERSION 2

#define ZWLR_LAYER_SURFACE_V1_SET_EXCLUSIVE_EDGE_SINCE_VERSION 5

static inline void
zwlr_layer_surface_v1_set_user_data(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwlr_layer_surface_v1, user_data);
}

static inline void *
zwlr_layer_surface_v1_get_user_data(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwlr_layer_surface_v1);
}

static inline uint32_t
zwlr_layer_surface_v1_get_version(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1);
}

static inline void
zwlr_layer_surface_v1_set_size(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t width, uint32_t height)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_SIZE, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, width, height);
}

static inline void
zwlr_layer_surface_v1_set_anchor(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t anchor)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_ANCHOR, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, anchor);
}

static inline void
zwlr_layer_surface_v1_set_exclusive_zone(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, int32_t zone)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_EXCLUSIVE_ZONE, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, zone);
}

static inline void
zwlr_layer_surface_v1_set_margin(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, int32_t top, int32_t right, int32_t bottom, int32_t left)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_MARGIN, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, top, right, bottom, left);
}

static inline void
zwlr_layer_surface_v1_set_keyboard_interactivity(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t keyboard_interactivity)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_KEYBOARD_INTERACTIVITY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, keyboard_interactivity);
}

static inline void
zwlr_layer_surface_v1_get_popup(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, struct xdg_popup *popup)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_GET_POPUP, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, popup);
}

static inline void
zwlr_layer_surface_v1_ack_configure(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t serial)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_ACK_CONFIGURE, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, serial);
}

static inline void
zwlr_layer_surface_v1_destroy(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
zwlr_layer_surface_v1_set_layer(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t layer)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_LAYER, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, layer);
}

static inline void
zwlr_layer_surface_v1_set_exclusive_edge(struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t edge)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_layer_surface_v1,
			 ZWLR_LAYER_SURFACE_V1_SET_EXCLUSIVE_EDGE, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_layer_surface_v1), 0, edge);
}

struct wl_buffer;
struct wl_output;
struct zwlr_screencopy_frame_v1;
struct zwlr_screencopy_manager_v1;

#ifndef ZWLR_SCREENCOPY_MANAGER_V1_INTERFACE
#define ZWLR_SCREENCOPY_MANAGER_V1_INTERFACE

extern const struct wl_interface gm_zwlr_screencopy_manager_v1_interface;
#endif
#ifndef ZWLR_SCREENCOPY_FRAME_V1_INTERFACE
#define ZWLR_SCREENCOPY_FRAME_V1_INTERFACE

extern const struct wl_interface gm_zwlr_screencopy_frame_v1_interface;
#endif

#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT 0
#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_REGION 1
#define ZWLR_SCREENCOPY_MANAGER_V1_DESTROY 2

#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_REGION_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_MANAGER_V1_DESTROY_SINCE_VERSION 1

static inline void
zwlr_screencopy_manager_v1_set_user_data(struct zwlr_screencopy_manager_v1 *zwlr_screencopy_manager_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwlr_screencopy_manager_v1, user_data);
}

static inline void *
zwlr_screencopy_manager_v1_get_user_data(struct zwlr_screencopy_manager_v1 *zwlr_screencopy_manager_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwlr_screencopy_manager_v1);
}

static inline uint32_t
zwlr_screencopy_manager_v1_get_version(struct zwlr_screencopy_manager_v1 *zwlr_screencopy_manager_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_manager_v1);
}

static inline struct zwlr_screencopy_frame_v1 *
zwlr_screencopy_manager_v1_capture_output(struct zwlr_screencopy_manager_v1 *zwlr_screencopy_manager_v1, int32_t overlay_cursor, struct wl_output *output)
{
	struct wl_proxy *frame;

	frame = wl_proxy_marshal_flags((struct wl_proxy *) zwlr_screencopy_manager_v1,
			 ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT, &gm_zwlr_screencopy_frame_v1_interface, wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_manager_v1), 0, NULL, overlay_cursor, output);

	return (struct zwlr_screencopy_frame_v1 *) frame;
}

static inline struct zwlr_screencopy_frame_v1 *
zwlr_screencopy_manager_v1_capture_output_region(struct zwlr_screencopy_manager_v1 *zwlr_screencopy_manager_v1, int32_t overlay_cursor, struct wl_output *output, int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_proxy *frame;

	frame = wl_proxy_marshal_flags((struct wl_proxy *) zwlr_screencopy_manager_v1,
			 ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_REGION, &gm_zwlr_screencopy_frame_v1_interface, wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_manager_v1), 0, NULL, overlay_cursor, output, x, y, width, height);

	return (struct zwlr_screencopy_frame_v1 *) frame;
}

static inline void
zwlr_screencopy_manager_v1_destroy(struct zwlr_screencopy_manager_v1 *zwlr_screencopy_manager_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_screencopy_manager_v1,
			 ZWLR_SCREENCOPY_MANAGER_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_manager_v1), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef ZWLR_SCREENCOPY_FRAME_V1_ERROR_ENUM
#define ZWLR_SCREENCOPY_FRAME_V1_ERROR_ENUM
enum zwlr_screencopy_frame_v1_error {

	ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED = 0,

	ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER = 1,
};
#endif

#ifndef ZWLR_SCREENCOPY_FRAME_V1_FLAGS_ENUM
#define ZWLR_SCREENCOPY_FRAME_V1_FLAGS_ENUM
enum zwlr_screencopy_frame_v1_flags {

	ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT = 1,
};
#endif

struct zwlr_screencopy_frame_v1_listener {

	void (*buffer)(void *data,
		       struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
		       uint32_t format,
		       uint32_t width,
		       uint32_t height,
		       uint32_t stride);

	void (*flags)(void *data,
		      struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
		      uint32_t flags);

	void (*ready)(void *data,
		      struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
		      uint32_t tv_sec_hi,
		      uint32_t tv_sec_lo,
		      uint32_t tv_nsec);

	void (*failed)(void *data,
		       struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1);

	void (*damage)(void *data,
		       struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
		       uint32_t x,
		       uint32_t y,
		       uint32_t width,
		       uint32_t height);

	void (*linux_dmabuf)(void *data,
			     struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
			     uint32_t format,
			     uint32_t width,
			     uint32_t height);

	void (*buffer_done)(void *data,
			    struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1);
};

static inline int
zwlr_screencopy_frame_v1_add_listener(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1,
				      const struct zwlr_screencopy_frame_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwlr_screencopy_frame_v1,
				     (void (**)(void)) listener, data);
}

#define ZWLR_SCREENCOPY_FRAME_V1_COPY 0
#define ZWLR_SCREENCOPY_FRAME_V1_DESTROY 1
#define ZWLR_SCREENCOPY_FRAME_V1_COPY_WITH_DAMAGE 2

#define ZWLR_SCREENCOPY_FRAME_V1_BUFFER_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_FRAME_V1_FLAGS_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_FRAME_V1_READY_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_FRAME_V1_FAILED_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_FRAME_V1_DAMAGE_SINCE_VERSION 2

#define ZWLR_SCREENCOPY_FRAME_V1_LINUX_DMABUF_SINCE_VERSION 3

#define ZWLR_SCREENCOPY_FRAME_V1_BUFFER_DONE_SINCE_VERSION 3

#define ZWLR_SCREENCOPY_FRAME_V1_COPY_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_FRAME_V1_DESTROY_SINCE_VERSION 1

#define ZWLR_SCREENCOPY_FRAME_V1_COPY_WITH_DAMAGE_SINCE_VERSION 2

static inline void
zwlr_screencopy_frame_v1_set_user_data(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwlr_screencopy_frame_v1, user_data);
}

static inline void *
zwlr_screencopy_frame_v1_get_user_data(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwlr_screencopy_frame_v1);
}

static inline uint32_t
zwlr_screencopy_frame_v1_get_version(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_frame_v1);
}

static inline void
zwlr_screencopy_frame_v1_copy(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1, struct wl_buffer *buffer)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_screencopy_frame_v1,
			 ZWLR_SCREENCOPY_FRAME_V1_COPY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_frame_v1), 0, buffer);
}

static inline void
zwlr_screencopy_frame_v1_destroy(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_screencopy_frame_v1,
			 ZWLR_SCREENCOPY_FRAME_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_frame_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
zwlr_screencopy_frame_v1_copy_with_damage(struct zwlr_screencopy_frame_v1 *zwlr_screencopy_frame_v1, struct wl_buffer *buffer)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_screencopy_frame_v1,
			 ZWLR_SCREENCOPY_FRAME_V1_COPY_WITH_DAMAGE, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_screencopy_frame_v1), 0, buffer);
}

struct wl_output;
struct zxdg_output_manager_v1;
struct zxdg_output_v1;

#ifndef ZXDG_OUTPUT_MANAGER_V1_INTERFACE
#define ZXDG_OUTPUT_MANAGER_V1_INTERFACE

extern const struct wl_interface gm_zxdg_output_manager_v1_interface;
#endif
#ifndef ZXDG_OUTPUT_V1_INTERFACE
#define ZXDG_OUTPUT_V1_INTERFACE

extern const struct wl_interface gm_zxdg_output_v1_interface;
#endif

#define ZXDG_OUTPUT_MANAGER_V1_DESTROY 0
#define ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT 1

#define ZXDG_OUTPUT_MANAGER_V1_DESTROY_SINCE_VERSION 1

#define ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT_SINCE_VERSION 1

static inline void
zxdg_output_manager_v1_set_user_data(struct zxdg_output_manager_v1 *zxdg_output_manager_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zxdg_output_manager_v1, user_data);
}

static inline void *
zxdg_output_manager_v1_get_user_data(struct zxdg_output_manager_v1 *zxdg_output_manager_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zxdg_output_manager_v1);
}

static inline uint32_t
zxdg_output_manager_v1_get_version(struct zxdg_output_manager_v1 *zxdg_output_manager_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zxdg_output_manager_v1);
}

static inline void
zxdg_output_manager_v1_destroy(struct zxdg_output_manager_v1 *zxdg_output_manager_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zxdg_output_manager_v1,
			 ZXDG_OUTPUT_MANAGER_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zxdg_output_manager_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline struct zxdg_output_v1 *
zxdg_output_manager_v1_get_xdg_output(struct zxdg_output_manager_v1 *zxdg_output_manager_v1, struct wl_output *output)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) zxdg_output_manager_v1,
			 ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT, &gm_zxdg_output_v1_interface, wl_proxy_get_version((struct wl_proxy *) zxdg_output_manager_v1), 0, NULL, output);

	return (struct zxdg_output_v1 *) id;
}

struct zxdg_output_v1_listener {

	void (*logical_position)(void *data,
				 struct zxdg_output_v1 *zxdg_output_v1,
				 int32_t x,
				 int32_t y);

	void (*logical_size)(void *data,
			     struct zxdg_output_v1 *zxdg_output_v1,
			     int32_t width,
			     int32_t height);

	void (*done)(void *data,
		     struct zxdg_output_v1 *zxdg_output_v1);

	void (*name)(void *data,
		     struct zxdg_output_v1 *zxdg_output_v1,
		     const char *name);

	void (*description)(void *data,
			    struct zxdg_output_v1 *zxdg_output_v1,
			    const char *description);
};

static inline int
zxdg_output_v1_add_listener(struct zxdg_output_v1 *zxdg_output_v1,
			    const struct zxdg_output_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zxdg_output_v1,
				     (void (**)(void)) listener, data);
}

#define ZXDG_OUTPUT_V1_DESTROY 0

#define ZXDG_OUTPUT_V1_LOGICAL_POSITION_SINCE_VERSION 1

#define ZXDG_OUTPUT_V1_LOGICAL_SIZE_SINCE_VERSION 1

#define ZXDG_OUTPUT_V1_DONE_SINCE_VERSION 1

#define ZXDG_OUTPUT_V1_NAME_SINCE_VERSION 2

#define ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION 2

#define ZXDG_OUTPUT_V1_DESTROY_SINCE_VERSION 1

static inline void
zxdg_output_v1_set_user_data(struct zxdg_output_v1 *zxdg_output_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zxdg_output_v1, user_data);
}

static inline void *
zxdg_output_v1_get_user_data(struct zxdg_output_v1 *zxdg_output_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zxdg_output_v1);
}

static inline uint32_t
zxdg_output_v1_get_version(struct zxdg_output_v1 *zxdg_output_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zxdg_output_v1);
}

static inline void
zxdg_output_v1_destroy(struct zxdg_output_v1 *zxdg_output_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zxdg_output_v1,
			 ZXDG_OUTPUT_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zxdg_output_v1), WL_MARSHAL_FLAG_DESTROY);
}

extern const struct wl_interface gm_wl_buffer_interface;
extern const struct wl_interface gm_wl_callback_interface;
extern const struct wl_interface gm_wl_keyboard_interface;
extern const struct wl_interface gm_wl_output_interface;
extern const struct wl_interface gm_wl_pointer_interface;
extern const struct wl_interface gm_wl_region_interface;
extern const struct wl_interface gm_wl_registry_interface;
extern const struct wl_interface gm_wl_shm_pool_interface;
extern const struct wl_interface gm_wl_surface_interface;
extern const struct wl_interface gm_wl_touch_interface;

static const struct wl_interface *gm_wayland_types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&gm_wl_callback_interface,
	&gm_wl_registry_interface,
	&gm_wl_surface_interface,
	&gm_wl_region_interface,
	&gm_wl_buffer_interface,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&gm_wl_shm_pool_interface,
	NULL,
	NULL,
	&gm_wl_buffer_interface,
	NULL,
	NULL,
	&gm_wl_callback_interface,
	&gm_wl_region_interface,
	&gm_wl_region_interface,
	&gm_wl_output_interface,
	&gm_wl_output_interface,
	&gm_wl_pointer_interface,
	&gm_wl_keyboard_interface,
	&gm_wl_touch_interface,
	NULL,
	&gm_wl_surface_interface,
	NULL,
	NULL,
	NULL,
	&gm_wl_surface_interface,
	NULL,
	NULL,
	NULL,
	&gm_wl_surface_interface,
	NULL,
	&gm_wl_surface_interface,
	NULL,
	NULL,
	&gm_wl_surface_interface,
	NULL,
	NULL,
	&gm_wl_surface_interface,
	NULL,
	NULL,
	NULL,
};

static const struct wl_message wl_display_requests[] = {
	{ "sync", "n", gm_wayland_types + 8 },
	{ "get_registry", "n", gm_wayland_types + 9 },
};

static const struct wl_message wl_display_events[] = {
	{ "error", "ous", gm_wayland_types + 0 },
	{ "delete_id", "u", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_display_interface = {
	"wl_display", 1,
	2, wl_display_requests,
	2, wl_display_events,
};

static const struct wl_message wl_registry_requests[] = {
	{ "bind", "usun", gm_wayland_types + 0 },
};

static const struct wl_message wl_registry_events[] = {
	{ "global", "usu", gm_wayland_types + 0 },
	{ "global_remove", "u", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_registry_interface = {
	"wl_registry", 1,
	1, wl_registry_requests,
	2, wl_registry_events,
};

static const struct wl_message wl_callback_events[] = {
	{ "done", "u", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_callback_interface = {
	"wl_callback", 1,
	0, NULL,
	1, wl_callback_events,
};

static const struct wl_message wl_compositor_requests[] = {
	{ "create_surface", "n", gm_wayland_types + 10 },
	{ "create_region", "n", gm_wayland_types + 11 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_compositor_interface = {
	"wl_compositor", 6,
	2, wl_compositor_requests,
	0, NULL,
};

static const struct wl_message wl_shm_pool_requests[] = {
	{ "create_buffer", "niiiiu", gm_wayland_types + 12 },
	{ "destroy", "", gm_wayland_types + 0 },
	{ "resize", "i", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_shm_pool_interface = {
	"wl_shm_pool", 2,
	3, wl_shm_pool_requests,
	0, NULL,
};

static const struct wl_message wl_shm_requests[] = {
	{ "create_pool", "nhi", gm_wayland_types + 18 },
	{ "release", "2", gm_wayland_types + 0 },
};

static const struct wl_message wl_shm_events[] = {
	{ "format", "u", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_shm_interface = {
	"wl_shm", 2,
	2, wl_shm_requests,
	1, wl_shm_events,
};

static const struct wl_message wl_buffer_requests[] = {
	{ "destroy", "", gm_wayland_types + 0 },
};

static const struct wl_message wl_buffer_events[] = {
	{ "release", "", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_buffer_interface = {
	"wl_buffer", 1,
	1, wl_buffer_requests,
	1, wl_buffer_events,
};

static const struct wl_message wl_surface_requests[] = {
	{ "destroy", "", gm_wayland_types + 0 },
	{ "attach", "?oii", gm_wayland_types + 21 },
	{ "damage", "iiii", gm_wayland_types + 0 },
	{ "frame", "n", gm_wayland_types + 24 },
	{ "set_opaque_region", "?o", gm_wayland_types + 25 },
	{ "set_input_region", "?o", gm_wayland_types + 26 },
	{ "commit", "", gm_wayland_types + 0 },
	{ "set_buffer_transform", "2i", gm_wayland_types + 0 },
	{ "set_buffer_scale", "3i", gm_wayland_types + 0 },
	{ "damage_buffer", "4iiii", gm_wayland_types + 0 },
	{ "offset", "5ii", gm_wayland_types + 0 },
};

static const struct wl_message wl_surface_events[] = {
	{ "enter", "o", gm_wayland_types + 27 },
	{ "leave", "o", gm_wayland_types + 28 },
	{ "preferred_buffer_scale", "6i", gm_wayland_types + 0 },
	{ "preferred_buffer_transform", "6u", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_surface_interface = {
	"wl_surface", 6,
	11, wl_surface_requests,
	4, wl_surface_events,
};

static const struct wl_message wl_seat_requests[] = {
	{ "get_pointer", "n", gm_wayland_types + 29 },
	{ "get_keyboard", "n", gm_wayland_types + 30 },
	{ "get_touch", "n", gm_wayland_types + 31 },
	{ "release", "5", gm_wayland_types + 0 },
};

static const struct wl_message wl_seat_events[] = {
	{ "capabilities", "u", gm_wayland_types + 0 },
	{ "name", "2s", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_seat_interface = {
	"wl_seat", 10,
	4, wl_seat_requests,
	2, wl_seat_events,
};

static const struct wl_message wl_pointer_requests[] = {
	{ "set_cursor", "u?oii", gm_wayland_types + 32 },
	{ "release", "3", gm_wayland_types + 0 },
};

static const struct wl_message wl_pointer_events[] = {
	{ "enter", "uoff", gm_wayland_types + 36 },
	{ "leave", "uo", gm_wayland_types + 40 },
	{ "motion", "uff", gm_wayland_types + 0 },
	{ "button", "uuuu", gm_wayland_types + 0 },
	{ "axis", "uuf", gm_wayland_types + 0 },
	{ "frame", "5", gm_wayland_types + 0 },
	{ "axis_source", "5u", gm_wayland_types + 0 },
	{ "axis_stop", "5uu", gm_wayland_types + 0 },
	{ "axis_discrete", "5ui", gm_wayland_types + 0 },
	{ "axis_value120", "8ui", gm_wayland_types + 0 },
	{ "axis_relative_direction", "9uu", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_pointer_interface = {
	"wl_pointer", 10,
	2, wl_pointer_requests,
	11, wl_pointer_events,
};

static const struct wl_message wl_keyboard_requests[] = {
	{ "release", "3", gm_wayland_types + 0 },
};

static const struct wl_message wl_keyboard_events[] = {
	{ "keymap", "uhu", gm_wayland_types + 0 },
	{ "enter", "uoa", gm_wayland_types + 42 },
	{ "leave", "uo", gm_wayland_types + 45 },
	{ "key", "uuuu", gm_wayland_types + 0 },
	{ "modifiers", "uuuuu", gm_wayland_types + 0 },
	{ "repeat_info", "4ii", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_keyboard_interface = {
	"wl_keyboard", 10,
	1, wl_keyboard_requests,
	6, wl_keyboard_events,
};

static const struct wl_message wl_touch_requests[] = {
	{ "release", "3", gm_wayland_types + 0 },
};

static const struct wl_message wl_touch_events[] = {
	{ "down", "uuoiff", gm_wayland_types + 47 },
	{ "up", "uui", gm_wayland_types + 0 },
	{ "motion", "uiff", gm_wayland_types + 0 },
	{ "frame", "", gm_wayland_types + 0 },
	{ "cancel", "", gm_wayland_types + 0 },
	{ "shape", "6iff", gm_wayland_types + 0 },
	{ "orientation", "6if", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_touch_interface = {
	"wl_touch", 10,
	1, wl_touch_requests,
	7, wl_touch_events,
};

static const struct wl_message wl_output_requests[] = {
	{ "release", "3", gm_wayland_types + 0 },
};

static const struct wl_message wl_output_events[] = {
	{ "geometry", "iiiiissi", gm_wayland_types + 0 },
	{ "mode", "uiii", gm_wayland_types + 0 },
	{ "done", "2", gm_wayland_types + 0 },
	{ "scale", "2i", gm_wayland_types + 0 },
	{ "name", "4s", gm_wayland_types + 0 },
	{ "description", "4s", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_output_interface = {
	"wl_output", 4,
	1, wl_output_requests,
	6, wl_output_events,
};

static const struct wl_message wl_region_requests[] = {
	{ "destroy", "", gm_wayland_types + 0 },
	{ "add", "iiii", gm_wayland_types + 0 },
	{ "subtract", "iiii", gm_wayland_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_wl_region_interface = {
	"wl_region", 1,
	3, wl_region_requests,
	0, NULL,
};

extern const struct wl_interface gm_wl_output_interface;
extern const struct wl_interface gm_wl_surface_interface;
extern const struct wl_interface gm_xdg_popup_interface;
extern const struct wl_interface gm_zwlr_layer_surface_v1_interface;

static const struct wl_interface *gm_wlr_layer_shell_unstable_v1_types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&gm_zwlr_layer_surface_v1_interface,
	&gm_wl_surface_interface,
	&gm_wl_output_interface,
	NULL,
	NULL,
	&gm_xdg_popup_interface,
};

static const struct wl_message zwlr_layer_shell_v1_requests[] = {
	{ "get_layer_surface", "no?ous", gm_wlr_layer_shell_unstable_v1_types + 4 },
	{ "destroy", "3", gm_wlr_layer_shell_unstable_v1_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_zwlr_layer_shell_v1_interface = {
	"zwlr_layer_shell_v1", 5,
	2, zwlr_layer_shell_v1_requests,
	0, NULL,
};

static const struct wl_message zwlr_layer_surface_v1_requests[] = {
	{ "set_size", "uu", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "set_anchor", "u", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "set_exclusive_zone", "i", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "set_margin", "iiii", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "set_keyboard_interactivity", "u", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "get_popup", "o", gm_wlr_layer_shell_unstable_v1_types + 9 },
	{ "ack_configure", "u", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "destroy", "", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "set_layer", "2u", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "set_exclusive_edge", "5u", gm_wlr_layer_shell_unstable_v1_types + 0 },
};

static const struct wl_message zwlr_layer_surface_v1_events[] = {
	{ "configure", "uuu", gm_wlr_layer_shell_unstable_v1_types + 0 },
	{ "closed", "", gm_wlr_layer_shell_unstable_v1_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_zwlr_layer_surface_v1_interface = {
	"zwlr_layer_surface_v1", 5,
	10, zwlr_layer_surface_v1_requests,
	2, zwlr_layer_surface_v1_events,
};

extern const struct wl_interface gm_wl_buffer_interface;
extern const struct wl_interface gm_wl_output_interface;
extern const struct wl_interface gm_zwlr_screencopy_frame_v1_interface;

static const struct wl_interface *gm_wlr_screencopy_unstable_v1_types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&gm_zwlr_screencopy_frame_v1_interface,
	NULL,
	&gm_wl_output_interface,
	&gm_zwlr_screencopy_frame_v1_interface,
	NULL,
	&gm_wl_output_interface,
	NULL,
	NULL,
	NULL,
	NULL,
	&gm_wl_buffer_interface,
	&gm_wl_buffer_interface,
};

static const struct wl_message zwlr_screencopy_manager_v1_requests[] = {
	{ "capture_output", "nio", gm_wlr_screencopy_unstable_v1_types + 4 },
	{ "capture_output_region", "nioiiii", gm_wlr_screencopy_unstable_v1_types + 7 },
	{ "destroy", "", gm_wlr_screencopy_unstable_v1_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_zwlr_screencopy_manager_v1_interface = {
	"zwlr_screencopy_manager_v1", 3,
	3, zwlr_screencopy_manager_v1_requests,
	0, NULL,
};

static const struct wl_message zwlr_screencopy_frame_v1_requests[] = {
	{ "copy", "o", gm_wlr_screencopy_unstable_v1_types + 14 },
	{ "destroy", "", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "copy_with_damage", "2o", gm_wlr_screencopy_unstable_v1_types + 15 },
};

static const struct wl_message zwlr_screencopy_frame_v1_events[] = {
	{ "buffer", "uuuu", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "flags", "u", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "ready", "uuu", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "failed", "", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "damage", "2uuuu", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "linux_dmabuf", "3uuu", gm_wlr_screencopy_unstable_v1_types + 0 },
	{ "buffer_done", "3", gm_wlr_screencopy_unstable_v1_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_zwlr_screencopy_frame_v1_interface = {
	"zwlr_screencopy_frame_v1", 3,
	3, zwlr_screencopy_frame_v1_requests,
	7, zwlr_screencopy_frame_v1_events,
};

extern const struct wl_interface gm_wl_output_interface;
extern const struct wl_interface gm_zxdg_output_v1_interface;

static const struct wl_interface *gm_xdg_output_unstable_v1_types[] = {
	NULL,
	NULL,
	&gm_zxdg_output_v1_interface,
	&gm_wl_output_interface,
};

static const struct wl_message zxdg_output_manager_v1_requests[] = {
	{ "destroy", "", gm_xdg_output_unstable_v1_types + 0 },
	{ "get_xdg_output", "no", gm_xdg_output_unstable_v1_types + 2 },
};

GM_WL_PRIVATE const struct wl_interface gm_zxdg_output_manager_v1_interface = {
	"zxdg_output_manager_v1", 3,
	2, zxdg_output_manager_v1_requests,
	0, NULL,
};

static const struct wl_message zxdg_output_v1_requests[] = {
	{ "destroy", "", gm_xdg_output_unstable_v1_types + 0 },
};

static const struct wl_message zxdg_output_v1_events[] = {
	{ "logical_position", "ii", gm_xdg_output_unstable_v1_types + 0 },
	{ "logical_size", "ii", gm_xdg_output_unstable_v1_types + 0 },
	{ "done", "", gm_xdg_output_unstable_v1_types + 0 },
	{ "name", "2s", gm_xdg_output_unstable_v1_types + 0 },
	{ "description", "2s", gm_xdg_output_unstable_v1_types + 0 },
};

GM_WL_PRIVATE const struct wl_interface gm_zxdg_output_v1_interface = {
	"zxdg_output_v1", 3,
	1, zxdg_output_v1_requests,
	5, zxdg_output_v1_events,
};

// layer-shell get_popup points at it, we never make popups
GM_WL_PRIVATE const struct wl_interface gm_xdg_popup_interface = {"xdg_popup", 1, 0, NULL, 0, NULL};

#endif
