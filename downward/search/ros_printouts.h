#ifndef ROS_PRINTOUTS_H
#define ROS_PRINTOUTS_H

/// Compatibility header for non-ros build

#if ROS_BUILD
    // Nothing to do, use the ROS functionality
#else

    #define ROS_DEBUG_STREAM(args) \
    cout << args << endl;

    #define ROS_INFO_STREAM(args) \
    cout << args << endl;


    #define ROS_DEBUG \
        printf 

    #define ROS_INFO \
        printf 

    #define ROS_WARN \
        printf 

    #define ROS_ERROR \
        printf 

    #define ROS_FATAL \
        printf

    #define ROS_ASSERT assert

#endif

#endif

