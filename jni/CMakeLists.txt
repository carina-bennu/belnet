add_library(belnet-android
    SHARED
    belnet_config.cpp
    belnet_daemon.cpp)
target_link_libraries(belnet-android libbelnet)
