# Mali Vulkan SDK for Android

![Mali Vulkan SDK banner](http://malideveloper.arm.com/wp-content/uploads/2016/03/vulkanSDKbanner.png)

## Introduction

The Mali Vulkan Software Development Kit is a collection of resources to help you build Vulkan applications
for a platform with a Mali GPU and an ARM processor.
You can use it for creating new applications, training, and exploration of implementation possibilities.

## Setting up development environment

### Minimum NDK requirements

Minimum android-ndk-r12 is required. The NDK can be downloaded from Android Studio 2.2 or later.
It is recommended to use the NDK provided in Android Studio.

### OS requirements

The Mali Vulkan SDK has been tested to build on Linux (Android Studio) and Windows (Android Studio).
Partial support for running on Linux desktop is also included.

### Android requirements

Not all Android phones will support Vulkan due to the GPU chipset on the device. To make sure your Android device has the Vulkan API supported download the [Hardware CapsViewer for Vulkan](https://play.google.com/store/apps/details?id=de.saschawillems.vulkancapsviewer&hl=en) app to verify your device's statuss.

### License

The software is provided under an MIT license. Contributions to this project are accepted under the same license.

### Building

#### Check out submodules

This repo uses GLM and STB as submodules, before building, make sure you pull those in.

```
git submodule init
git submodule update
```

#### Build and run samples from Android Studio

  - Open Android Studio 2.2 or newer
  - Open an existing Android Studio project
  - Import a sample project
  - You might be prompted to update or install the Gradle wrapper. Do so if asked.
  - You might be prompted to download and/or update Android SDK tools if Android Studio has not downloaded these before.
  - Under Tools -> Android -> SDK manager, install cmake, lldb and NDK components if these are not installed already.
  - In the top menu, run Build -> Make Project.
  - In the bottom of the screen, the Messages tab should display a build log which shows that libnative.so has been built and that build was successful.
  - Run the app on the device by pressing the Play button on the top toolbar.
  - To debug the code, Run -> Debug app. In the project view, you will find app/cpp/native/hellotriangle.cpp or similar. You can set breakpoints and step through the code.

#### Build samples for desktop Linux

```
mkdir build
cd build
cmake ..
make -j8
```
will build samples with a PNG backend. Running the binary on desktop should dump out PNG images.
This is useful when developing samples and for creating screenshots.

X11 or Wayland backends can be used instead on Linux by passing in extra parameters to cmake:

```
cmake .. -DPLATFORM=wayland   # or xcb for X11
```

#### Documentation

For online tutorials, documentation and explanation of the samples,
see [Mali Vulkan SDK documentation](http://malideveloper.arm.com/downloads/deved/tutorial/SDK/Vulkan/1.0/index.html).

To build the same documentation for yourself for offline use, build Doxygen documentation with `./build_documentation.sh`.
This requires Doxygen to be installed on your machine.

## Adding new samples

The build system for samples is designed to be as general as possible. To create a new sample based on hellotriangle:

```
cd samples
$EDITOR CMakeLists.txt             # add_subdirectory(newsample)
mkdir newsample
cp -r hellotriangle/{CMakeLists.txt,app,build.gradle,settings.gradle} newsample/
$EDITOR CMakeLists.txt             # Edit add_sample
$EDITOR app/AndroidManifest.xml    # Edit manifest:package
$EDITOR app/res/values/strings.xml # Edit resources:string
```

Source files go in `newsample/`,
GLSL source files go in `newsample/shaders` and general assets (if needed) go in
`newsample/assets`.

Samples must implement the `VulkanApplication` interface as well as implementing `MaliSDK::create_application()`.
```
#include "framework/application.hpp"
#include "framework/context.hpp"
#include "framework/common.hpp"
#include "framework/assets.hpp"
#include "platform/platform.hpp"

class MyVulkanApplication : public VulkanApplication
{
    // ...
};

VulkanApplication* MaliSDK::create_application()
{
    return new MyVulkanApplication();
}
```

