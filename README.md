# sbus_ros2
This is a sbus converter workspace include 2 ros packages for ROS2.  
This repositories is forked from [sbus_ros2](https://github.com/eryeden/sbus_ros2). Change some params to fit [sbus2uart](https://item.taobao.com/item.htm?spm=a230r.1.14.46.3e531953EdT2KZ&id=675284580820&ns=1&abbucket=9#detail) modules.

## Build
```bash
colcon build --symlink-install
```

## Launch the node
```bash
ros2 launch sbus_bridge sbus_bridge_node_launch.py
```

## Config
See `src/sbus_bridge/config/params.yaml`
