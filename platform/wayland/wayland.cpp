/* Copyright (c) 2016-2017, ARM Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "wayland.hpp"
#include <fcntl.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

namespace MaliSDK
{
Platform &Platform::get()
{
	// Not initialized until first call to Platform::get().
	// Initialization is thread-safe.
	static WaylandPlatform singleton;
	return singleton;
}

VkSurfaceKHR WaylandPlatform::createSurface()
{
	if (FAILED(initWindow()))
		return VK_NULL_HANDLE;

	VkSurfaceKHR surface;
	PFN_vkCreateWaylandSurfaceKHR fpCreateWaylandSurfaceKHR;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateWaylandSurfaceKHR", fpCreateWaylandSurfaceKHR))
		return VK_NULL_HANDLE;

	VkWaylandSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
	info.display = waylandData.dpy;
	info.surface = waylandData.surf;

	VK_CHECK(fpCreateWaylandSurfaceKHR(instance, &info, nullptr, &surface));
	return surface;
}

WaylandPlatform::WaylandPlatform()
{
	memset(&waylandData, 0, sizeof(waylandData));
	waylandData.fd = -1;
}

// Wayland callbacks
static void registryHandleGlobal(void *data, wl_registry *reg, uint32_t id, const char *interface, uint32_t)
{
	WaylandData *waylandData = static_cast<WaylandData *>(data);
	if (!strcmp(interface, "wl_compositor"))
		waylandData->compositor = static_cast<wl_compositor *>(wl_registry_bind(reg, id, &wl_compositor_interface, 1));
	else if (!strcmp(interface, "wl_shell"))
		waylandData->shell = static_cast<wl_shell *>(wl_registry_bind(reg, id, &wl_shell_interface, 1));
}

static void registryHandleGlobalRemove(void *, wl_registry *, uint32_t)
{
}

static const wl_registry_listener registryListener = {
	registryHandleGlobal, registryHandleGlobalRemove,
};

static void shellSurfaceHandlePing(void *, wl_shell_surface *shellSurface, uint32_t serial)
{
	wl_shell_surface_pong(shellSurface, serial);
}

static void shellSurfaceHandlePopupDone(void *, wl_shell_surface *)
{
}

static void shellSurfaceHandleConfigure(void *, wl_shell_surface *, uint32_t, int32_t width, int32_t height)
{
	LOGI("Wayland: Surface size: %d x %d.\n", width, height);
}

static const wl_shell_surface_listener shellSurfaceListener = {
	shellSurfaceHandlePing, shellSurfaceHandleConfigure, shellSurfaceHandlePopupDone,
};

Platform::Status WaylandPlatform::getWindowStatus()
{
	return waylandData.status;
}

void WaylandPlatform::flushWaylandFd()
{
	pollfd fd = { 0 };
	wl_display_dispatch_pending(waylandData.dpy);
	wl_display_flush(waylandData.dpy);

	fd.fd = waylandData.fd;
	fd.events = POLLIN | POLLOUT | POLLERR | POLLHUP;

	if (poll(&fd, 1, 0) > 0)
	{
		if (fd.revents & (POLLERR | POLLHUP))
		{
			close(waylandData.fd);
			waylandData.fd = -1;
			waylandData.status = STATUS_TEARDOWN;
		}

		if (fd.revents & POLLIN)
			wl_display_dispatch(waylandData.dpy);
		if (fd.revents & POLLOUT)
			wl_display_flush(waylandData.dpy);
	}
}

Result WaylandPlatform::initialize()
{
	waylandData.dpy = wl_display_connect(nullptr);
	if (!waylandData.dpy)
		return RESULT_ERROR_IO;

	waylandData.registry = wl_display_get_registry(waylandData.dpy);
	wl_registry_add_listener(waylandData.registry, &registryListener, &waylandData);
	wl_display_roundtrip(waylandData.dpy);

	if (!waylandData.compositor || !waylandData.shell)
		return RESULT_ERROR_GENERIC;

	waylandData.fd = wl_display_get_fd(waylandData.dpy);
	if (waylandData.fd < 0)
		return RESULT_ERROR_IO;

	waylandData.status = STATUS_RUNNING;

	return WSIPlatform::initialize();
}

Platform::SwapchainDimensions WaylandPlatform::getPreferredSwapchain()
{
	SwapchainDimensions chain = { 1280, 720, VK_FORMAT_B8G8R8A8_UNORM };
	return chain;
}

Result WaylandPlatform::initWindow()
{
	waylandData.surf = wl_compositor_create_surface(waylandData.compositor);
	waylandData.shellSurf = wl_shell_get_shell_surface(waylandData.shell, waylandData.surf);

	if (!waylandData.surf || !waylandData.shellSurf)
		return RESULT_ERROR_GENERIC;

	wl_shell_surface_add_listener(waylandData.shellSurf, &shellSurfaceListener, &waylandData);
	wl_shell_surface_set_toplevel(waylandData.shellSurf);
	wl_shell_surface_set_class(waylandData.shellSurf, "Mali SDK");
	wl_shell_surface_set_title(waylandData.shellSurf, "Mali SDK");

	flushWaylandFd();
	return RESULT_SUCCESS;
}

Result WaylandPlatform::createWindow(const SwapchainDimensions &swapchain)
{
	if (!waylandData.dpy)
		return RESULT_ERROR_GENERIC;

	return initVulkan(swapchain, { "VK_KHR_surface", "VK_KHR_wayland_surface" }, { "VK_KHR_swapchain" });
}

void WaylandPlatform::terminate()
{
	// Tear down WSI resources before we destroy Wayland.
	WSIPlatform::terminate();

	if (waylandData.dpy)
	{
		if (waylandData.fd >= 0)
			close(waylandData.fd);

		if (waylandData.shellSurf)
			wl_shell_surface_destroy(waylandData.shellSurf);
		if (waylandData.surf)
			wl_surface_destroy(waylandData.surf);
		if (waylandData.compositor)
			wl_compositor_destroy(waylandData.compositor);
		if (waylandData.registry)
			wl_registry_destroy(waylandData.registry);
		if (waylandData.dpy)
			wl_display_disconnect(waylandData.dpy);

		waylandData.dpy = nullptr;
	}
}

WaylandPlatform::~WaylandPlatform()
{
	terminate();
}

Result WaylandPlatform::presentImage(unsigned index)
{
	Result res = WSIPlatform::presentImage(index);
	flushWaylandFd();
	return res;
}
}
