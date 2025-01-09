# ROSplane

This fork of the ROSplane repository, is an example of how to appropriately fork and modify ROSplane. A few of the key points are outlined below.

## Key Steps

- Move unchanged ROS packages to a directory that will not be built if you have the normal ROSplane built on your system as well. Having ROS packages with the same name can cause issues when running and building.
  - For example, in this repository, the `rosplane_extra`, `rosplane_gcs`, `rosplane_msgs`, `rosplane_sim` and `rosplane_tuning` ROS packages (folders) have been moved to the `.unchanged` directory.
- Rename the modified package for clarity and to avoid aforementioned build issues.
- For additional clarity archive or remove the unchanged/unused files within the modified ROSplane package. This will help when bugs inevitably arise. You can know if the issue originates in the original repository or is due to your changes.

[![ROS2 CI](https://github.com/rosflight/rosplane/actions/workflows/ros2-ci.yml/badge.svg)](https://github.com/rosflight/rosplane/actions/workflows/ros2-ci.yml)

ROSplane is a basic fixed-wing autopilot build around ROS2 for use with the ROSflight autopilot. It is a continuation of the original [ROSplane](https://github.com/byu-magicc/rosplane) project. It is built according to the methods published in Small Unmanned Aircraft: Theory and Practice by Dr. Randy Beard and Dr. Tim McLain.

For more information, see the [ROSplane overview](https://docs.rosflight.org/git-main/user-guide/rosplane-overview/) or the [ROSplane setup guide](https://docs.rosflight.org/git-main/user-guide/rosplane-setup/) on the [ROSflight documentation](https://docs.rosflight.org/git-main/).
