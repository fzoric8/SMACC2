#!/bin/bash
ros2 trace -u smacc_trace:smacc_event smacc_trace:client_behavior_on_exit_end smacc_trace:client_behavior_on_exit_start smacc_trace:client_behavior_on_entry_end smacc_trace:client_behavior_on_entry_start smacc_trace:client_behavior_on_runtimeConfigure_end smacc_trace:client_behavior_on_runtimeConfigure_start smacc_trace:state_onExit_end smacc_trace:state_onExit_start smacc_trace:state_onEntry_end smacc_trace:state_onEntry_start smacc_trace:update_end smacc_trace:update_start smacc_trace:spinOnce ros2:rcl_init ros2:rcl_node_init ros2:rcl_publisher_init ros2:rcl_publish ros2:rclcpp_publish ros2:rcl_subscription_init ros2:rclcpp_subscription_init ros2:rclcpp_subscription_callback_added ros2:rcl_service_init ros2:rclcpp_service_callback_added ros2:rcl_client_init ros2:rcl_timer_init ros2:rclcpp_timer_callback_added ros2:rclcpp_timer_link_node ros2:rclcpp_callback_register ros2:callback_start ros2:callback_end ros2:rcl_lifecycle_state_machine_init ros2:rcl_lifecycle_transition 