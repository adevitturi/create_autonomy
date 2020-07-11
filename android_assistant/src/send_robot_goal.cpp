#include <ros/ros.h>

#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <move_base_msgs/MoveBaseActionFeedback.h>
#include <move_base_msgs/MoveBaseActionResult.h>
#include "geometry_msgs/Vector3.h"
#include "std_msgs/Float32.h"
#include "std_msgs/String.h"

#include <math.h>
#include <iostream>
#include <sstream>
#include <string>

#define STATE_AWAIT_GOAL 0
#define STATE_PUBLISH_GOAL 1
#define STATE_GET_RESULT 2
#define STATE_PUBLISH_RESULT 3

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction>
    MoveBaseClient;
const std::string NODE_NAME = "navigation_goal";

int node_state = STATE_AWAIT_GOAL;
geometry_msgs::Vector3 goal;
std_msgs::Float32 distance;
std_msgs::String result;

void onNewGoalCallback(const geometry_msgs::Vector3::ConstPtr& msg) {
  if (node_state != STATE_AWAIT_GOAL) {
    return;
  }
  ROS_INFO("New goal received! (x = %.2f, y = %.2f)", msg->x, msg->y);
  goal.x = msg->x;
  goal.y = msg->y;
  goal.z = msg->z;
  node_state = STATE_PUBLISH_GOAL;
}

void onResultCallback(
    const move_base_msgs::MoveBaseActionResult::ConstPtr& msg) {
  if (msg->status.status == 3) {
    result.data = "Robot has arrived to the goal position";
    ROS_INFO("Robot has arrived to the goal position");
  } else {
    result.data = "The base failed for some reason";
    ROS_INFO("The base failed for some reason");
  }
  ROS_INFO("Waiting for a new goal...");
  node_state = STATE_PUBLISH_RESULT;
}

void onFeedbackCallback(
    const move_base_msgs::MoveBaseActionFeedback::ConstPtr& msg) {
  float deltaX = goal.x - msg->feedback.base_position.pose.position.x;
  float deltaY = goal.y - msg->feedback.base_position.pose.position.y;
  distance.data = sqrt(pow(deltaX, 2) + pow(deltaY, 2));
}

int main(int argc, char** argv) {
  ros::init(argc, argv, NODE_NAME);
  ros::NodeHandle n;
  MoveBaseClient ac("create1/move_base", true);

  ros::Subscriber sub_goal =
      n.subscribe("assistant_goal", 1000, onNewGoalCallback);
  ros::Subscriber sub_result =
      n.subscribe("create1/move_base/result", 1000, onResultCallback);
  ros::Subscriber sub_feedback =
      n.subscribe("create1/move_base/feedback", 1000, onFeedbackCallback);
  ros::Publisher pub_distance =
      n.advertise<std_msgs::Float32>("assistant_goal_distance", 1000);
  ros::Publisher pub_result =
      n.advertise<std_msgs::String>("assistant_goal_result", 1000);

  while (!ac.waitForServer(ros::Duration(5.0))) {
    ROS_INFO("Waiting for the move_base action server...");
  }

  // Frecuency of loop in Hz
  ros::Rate loop_rate(10);
  ROS_INFO("Waiting for a new goal...");

  while (ros::ok()) {
    if (node_state == STATE_PUBLISH_GOAL) {
      move_base_msgs::MoveBaseGoal move_base_goal;
      move_base_goal.target_pose.header.frame_id = "map";
      move_base_goal.target_pose.header.stamp = ros::Time::now();
      try {
        move_base_goal.target_pose.pose.position.x = goal.x;
        move_base_goal.target_pose.pose.position.y = goal.y;
      } catch (int e) {
        ROS_WARN_STREAM_NAMED(NODE_NAME,
                              "Using default 2D pose: [0 m, 0 m, 0 deg]");
        move_base_goal.target_pose.pose.position.x = 0.0;
        move_base_goal.target_pose.pose.position.y = 0.0;
      }
      move_base_goal.target_pose.pose.orientation.w = 1.0;
      ROS_INFO("Sending move base goal");
      ac.sendGoal(move_base_goal);
      node_state = STATE_GET_RESULT;
    }

    if (node_state == STATE_PUBLISH_RESULT) {
      pub_result.publish(result);
      node_state = STATE_AWAIT_GOAL;
    }
    // Publish the distance to the goal.
    pub_distance.publish(distance);

    // For callbacks
    ros::spinOnce();
    // Go to sleep for the configured interval
    loop_rate.sleep();
  }
  return 0;
}