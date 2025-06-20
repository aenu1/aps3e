# hidapi
set(BUILD_SHARED_LIBS FALSE CACHE BOOL "Don't build shared libs")
set(HIDAPI_INSTALL_TARGETS FALSE CACHE BOOL "Don't install anything")

if(CMAKE_SYSTEM MATCHES "Linux")
	set(HIDAPI_WITH_LIBUSB FALSE CACHE BOOL "Don't build with libusb for linux")
endif()

add_library(3rdparty_hidapi INTERFACE)
add_subdirectory(hidapi EXCLUDE_FROM_ALL)

if(APPLE)
	target_link_libraries(3rdparty_hidapi INTERFACE hidapi_darwin "-framework CoreFoundation" "-framework IOKit")
elseif(CMAKE_SYSTEM MATCHES "Linux")
	target_link_libraries(3rdparty_hidapi INTERFACE hidapi-hidraw udev)
elseif(WIN32)
	target_link_libraries(3rdparty_hidapi INTERFACE hidapi::hidapi hidapi::include Shlwapi.lib)
elseif(ANDROID)
    message(STATUS "hidapi skip libusb")
else()
	target_link_libraries(3rdparty_hidapi INTERFACE hidapi-libusb usb)
endif()
