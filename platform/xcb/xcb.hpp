#ifndef PLATFORM_XCB_HPP
#define PLATFORM_XCB_HPP

#include "platform.hpp"
#include "platform/os/linux.hpp"
#include "platform/wsi/wsi.hpp"

namespace MaliSDK
{

/// @brief The XCB specific platform. Inherits from WSIPlatform.
class XCBPlatform : public WSIPlatform
{
public:
	XCBPlatform();
	virtual ~XCBPlatform();

	/// @brief Initialize the platform.
	/// @returns Error code.
	virtual Result initialize() override;

	/// @brief Gets the preferred swapchain size.
	/// @returns Error code.
	virtual SwapchainDimensions getPreferredSwapchain() override;

	/// @brief Creates a window with desired swapchain dimensions.
	///
	/// The swapchain parameters might not necessarily be honored by the platform.
	/// Use @ref getCurrentSwapchain to query the dimensions we actually
	/// initialized.
	/// @returns Error code.
	virtual Result createWindow(const SwapchainDimensions &swapchain) override;

	/// @brief Gets current window status.
	/// @returns Window status.
	virtual Status getWindowStatus() override;

	/// @brief Presents an image to the swapchain.
	/// @param index The swapchain index previously obtained from @ref
	/// acquireNextImage.
	/// @returns Error code.
	virtual Result presentImage(unsigned index) override;

	/// @brief Terminates the platform.
	virtual void terminate() override;

private:
	xcb_connection_t *connection;
	xcb_window_t window;
	Platform::Status status;
	xcb_intern_atom_reply_t *atom_delete_window;

	void handleEvents();

	LinuxAssetManager assetManager;
	virtual VkSurfaceKHR createSurface() override;
};
}

#endif
