/*****************************************************************************************************************
 * ReelRobotix Inc. - Software License Agreement      Copyright (c) 2018
 * 	 Authors: Pablo Inigo Blasco, Brett Aldrich
 *
 ******************************************************************************************************************/
#include <angles/angles.h>
#include <forward_local_planner/forward_local_planner.h>
#include <tf2/utils.h>

#include <boost/intrusive_ptr.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <pluginlib/class_list_macros.hpp>
#include <rclcpp/rclcpp.hpp>
#include <visualization_msgs/msg/marker_array.hpp>

using namespace std::chrono_literals;
namespace cl_move_base_z
{
namespace forward_local_planner
{
/**
******************************************************************************************************************
* ForwardLocalPlanner()
******************************************************************************************************************
*/
ForwardLocalPlanner::ForwardLocalPlanner() : waitingTimeout_(2s)
{
}

ForwardLocalPlanner::~ForwardLocalPlanner()
{
}

void ForwardLocalPlanner::activate()
{
  RCLCPP_DEBUG_STREAM(nh_->get_logger(), "activating controller ForwardLocalPlanner");
  this->updateParameters();
  this->goalMarkerPublisher_->on_activate();
}

void ForwardLocalPlanner::deactivate()
{
}

void ForwardLocalPlanner::cleanup()
{
  this->plan_.clear();
  this->currentPoseIndex_ = 0;
}

void ForwardLocalPlanner::configure(const rclcpp_lifecycle::LifecycleNode::WeakPtr &node, std::string name,
                                    const std::shared_ptr<tf2_ros::Buffer> & /*tf*/,
                                    const std::shared_ptr<nav2_costmap_2d::Costmap2DROS> &costmap_ros)
{
  // nh_ = rclcpp::Node::make_shared("~/ForwardLocalPlanner");
  nh_ = node.lock();
  costmapRos_ = costmap_ros;
  name_ = name;
  k_rho_ = 1.0;
  k_alpha_ = -0.4;
  k_betta_ = -1.0;  // set to zero means that orientation is not important
  // k_betta_ = 1.0;
  // betta_offset_=0;

  goalReached_ = false;
  carrot_distance_ = 0.4;
  yaw_goal_tolerance_ = 0.05;
  xy_goal_tolerance_ = 0.10;
  max_linear_x_speed_ = 1.0;
  max_angular_z_speed_ = 2.0;

  // rclcpp::Node::SharedPtr private_nh("~");

  currentPoseIndex_ = 0;

  nh_->declare_parameter(name_ + ".k_rho", k_rho_);
  nh_->declare_parameter(name_ + ".k_alpha", k_alpha_);
  nh_->declare_parameter(name_ + ".k_betta", k_betta_);
  nh_->declare_parameter(name_ + ".carrot_distance", carrot_distance_);
  nh_->declare_parameter(name_ + ".yaw_goal_tolerance", yaw_goal_tolerance_);
  nh_->declare_parameter(name_ + ".xy_goal_tolerance", xy_goal_tolerance_);
  nh_->declare_parameter(name_ + ".max_linear_x_speed", max_linear_x_speed_);
  nh_->declare_parameter(name_ + ".max_angular_z_speed", max_angular_z_speed_);

  RCLCPP_DEBUG(nh_->get_logger(),
               "[ForwardLocalPlanner] max linear speed: %lf, max angular speed: %lf, k_rho: %lf, carrot_distance: "
               "%lf, ",
               max_linear_x_speed_, max_angular_z_speed_, k_rho_, carrot_distance_);
  goalMarkerPublisher_ =
      nh_->create_publisher<visualization_msgs::msg::MarkerArray>("forward_local_planner/carrot_goal_marker", 1);

  waiting_ = false;
  waitingTimeout_ = rclcpp::Duration(10s);
}

void ForwardLocalPlanner::updateParameters()
{
  nh_->get_parameter(name_ + ".k_rho", k_rho_);
  nh_->get_parameter(name_ + ".k_alpha", k_alpha_);
  nh_->get_parameter(name_ + ".k_betta", k_betta_);
  nh_->get_parameter(name_ + ".carrot_distance", carrot_distance_);
  nh_->get_parameter(name_ + ".yaw_goal_tolerance", yaw_goal_tolerance_);
  nh_->get_parameter(name_ + ".xy_goal_tolerance", xy_goal_tolerance_);
  nh_->get_parameter(name_ + ".max_linear_x_speed", max_linear_x_speed_);
  nh_->get_parameter(name_ + ".max_angular_z_speed", max_angular_z_speed_);

  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.k_rho: " << k_rho_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.k_alpha: "<< k_alpha_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.k_betta: " << k_betta_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.carrot_distance: " << carrot_distance_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.yaw_goal_tolerance:" << yaw_goal_tolerance_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.xy_goal_tolerance: " << xy_goal_tolerance_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.max_linear_x_speed:" << max_linear_x_speed_);
  RCLCPP_INFO_STREAM(nh_->get_logger(),"[BackwardLocalPlanner.max_angular_z_speed:" << max_angular_z_speed_);

  
}

void ForwardLocalPlanner::generateTrajectory(const Eigen::Vector3f &pos, const Eigen::Vector3f &vel, float maxdist,
                                             float maxanglediff, float maxtime, float dt,
                                             std::vector<Eigen::Vector3f> &outtraj)
{
  // simulate the trajectory and check for collisions, updating costs along the way
  bool end = false;
  float time = 0;
  Eigen::Vector3f currentpos = pos;
  int i = 0;
  while (!end)
  {
    // add the point to the trajectory so we can draw it later if we want
    // traj.addPoint(pos[0], pos[1], pos[2]);

    // if (continued_acceleration_) {
    //   //calculate velocities
    //   loop_vel = computeNewVelocities(sample_target_vel, loop_vel, limits_->getAccLimits(), dt);
    //   //RCLCPP_WARN_NAMED(nh_->get_logger(), "Generator", "Flag: %d, Loop_Vel %f, %f, %f", continued_acceleration_,
    //   loop_vel[0], loop_vel[1], loop_vel[2]);
    // }

    auto loop_vel = vel;
    // update the position of the robot using the velocities passed in
    auto newpos = computeNewPositions(currentpos, loop_vel, dt);

    auto dx = newpos[0] - currentpos[0];
    auto dy = newpos[1] - currentpos[1];
    float dist, angledist;

    // RCLCPP_DEBUG(nh_->get_logger(), "traj point %d", i);
    dist = sqrt(dx * dx + dy * dy);
    if (dist > maxdist)
    {
      end = true;
      // RCLCPP_DEBUG(nh_->get_logger(), "dist break: %f", dist);
    }
    else
    {
      // ouble from, double to
      angledist = fabs(angles::shortest_angular_distance(currentpos[2], newpos[2]));
      if (angledist > maxanglediff)
      {
        end = true;
        // RCLCPP_DEBUG(nh_->get_logger(), "angle dist break: %f", angledist);
      }
      else
      {
        outtraj.push_back(newpos);

        time += dt;
        if (time > maxtime)
        {
          end = true;
          // RCLCPP_DEBUG(nh_->get_logger(), "time break: %f", time);
        }

        // RCLCPP_DEBUG(nh_->get_logger(), "dist: %f, angledist: %f, time: %f", dist, angledist, time);
      }
    }

    currentpos = newpos;
    i++;
  }  // end for simulation steps
}

Eigen::Vector3f ForwardLocalPlanner::computeNewPositions(const Eigen::Vector3f &pos, const Eigen::Vector3f &vel,
                                                         double dt)
{
  Eigen::Vector3f new_pos = Eigen::Vector3f::Zero();
  new_pos[0] = pos[0] + (vel[0] * cos(pos[2]) + vel[1] * cos(M_PI_2 + pos[2])) * dt;
  new_pos[1] = pos[1] + (vel[0] * sin(pos[2]) + vel[1] * sin(M_PI_2 + pos[2])) * dt;
  new_pos[2] = pos[2] + vel[2] * dt;
  return new_pos;
}

/**
******************************************************************************************************************
* publishGoalMarker()
******************************************************************************************************************
*/
void ForwardLocalPlanner::publishGoalMarker(double x, double y, double phi)
{
  visualization_msgs::msg::Marker marker;

  marker.header.frame_id = this->costmapRos_->getGlobalFrameID();
  marker.header.stamp = nh_->now();
  marker.ns = "my_namespace2";
  marker.id = 0;
  marker.type = visualization_msgs::msg::Marker::ARROW;
  marker.action = visualization_msgs::msg::Marker::ADD;
  marker.pose.orientation.w = 1;

  marker.scale.x = 0.1;
  marker.scale.y = 0.3;
  marker.scale.z = 0.1;
  marker.color.a = 1.0;
  marker.color.r = 0;
  marker.color.g = 0;
  marker.color.b = 1.0;

  geometry_msgs::msg::Point start, end;
  start.x = x;
  start.y = y;

  end.x = x + 0.5 * cos(phi);
  end.y = y + 0.5 * sin(phi);

  marker.points.push_back(start);
  marker.points.push_back(end);

  visualization_msgs::msg::MarkerArray ma;
  ma.markers.push_back(marker);

  goalMarkerPublisher_->publish(ma);
}

void clamp(rclcpp::Node::SharedPtr nh_, geometry_msgs::msg::Twist &cmd_vel, double max_linear_x_speed_,
           double max_angular_z_speed_)
{
  if (max_angular_z_speed_ == 0 || max_linear_x_speed_ == 0)
    return;

  if (cmd_vel.angular.z == 0)
  {
    cmd_vel.linear.x = max_linear_x_speed_;
  }
  else
  {
    double kurvature = cmd_vel.linear.x / cmd_vel.angular.z;

    double linearAuthority = fabs(cmd_vel.linear.x / max_linear_x_speed_);
    double angularAuthority = fabs(cmd_vel.angular.z / max_angular_z_speed_);
    if (linearAuthority < angularAuthority)
    {
      // lets go to maximum linear speed
      cmd_vel.linear.x = max_linear_x_speed_;
      cmd_vel.angular.z = kurvature / max_linear_x_speed_;
      RCLCPP_WARN_STREAM(nh_->get_logger(), "k=" << kurvature << "lets go to maximum linear capacity: " << cmd_vel);
    }
    else
    {
      // lets go with maximum angular speed
      cmd_vel.angular.x = max_angular_z_speed_;
      cmd_vel.linear.x = kurvature * max_angular_z_speed_;
      RCLCPP_WARN_STREAM(nh_->get_logger(), "lets go to maximum angular capacity: " << cmd_vel);
    }
  }
}

/**
******************************************************************************************************************
* computeVelocityCommands()
******************************************************************************************************************
*/
geometry_msgs::msg::TwistStamped ForwardLocalPlanner::computeVelocityCommands(
    const geometry_msgs::msg::PoseStamped &currentPose, const geometry_msgs::msg::Twist & /*velocity*/)
{
  this->updateParameters();
  geometry_msgs::msg::TwistStamped cmd_vel;
  goalReached_ = false;
  RCLCPP_DEBUG(nh_->get_logger(), "[ForwardLocalPlanner] ----- COMUTE VELOCITY COMMAND LOCAL PLANNER ---");

  bool ok = false;
  while (!ok)
  {
    // iterate the point from the current position and ahead until reaching a new goal point in the path
    while (!ok && currentPoseIndex_ < (int)plan_.size())
    {
      auto &pose = plan_[currentPoseIndex_];
      const geometry_msgs::msg::Point &p = pose.pose.position;
      tf2::Quaternion q;
      tf2::fromMsg(pose.pose.orientation, q);

      // take error from the current position to the path point
      double dx = p.x - currentPose.pose.position.x;
      double dy = p.y - currentPose.pose.position.y;
      double dist = sqrt(dx * dx + dy * dy);

      double pangle = tf2::getYaw(q);
      double angle = tf2::getYaw(currentPose.pose.orientation);
      double angular_error = angles::shortest_angular_distance(pangle, angle);

      if (dist >= carrot_distance_ || fabs(angular_error) > 0.1)
      {
        // the target pose is enough different to be defined as a target
        ok = true;
        RCLCPP_DEBUG(nh_->get_logger(),
                     "current index: %d, carrot goal percentaje: %lf, dist: %lf, maxdist: %lf, angle_error: %lf",
                     currentPoseIndex_, 100.0 * currentPoseIndex_ / plan_.size(), dist, carrot_distance_,
                     angular_error);
      }
      else
      {
        currentPoseIndex_++;
      }
    }

    RCLCPP_DEBUG_STREAM(nh_->get_logger(), "[ForwardLocalPlanner] selected carrot pose index " << currentPoseIndex_
                                                                                               << "/" << plan_.size());

    if (currentPoseIndex_ >= (int)plan_.size())
    {
      // even the latest point is quite similar, then take the last since it is the final goal
      cmd_vel.twist.linear.x = 0;
      cmd_vel.twist.angular.z = 0;
      // RCLCPP_INFO(nh_->get_logger(), "End Local planner");
      ok = true;
      currentPoseIndex_ = (int)plan_.size() - 1;
      // return true;
    }
  }

  // RCLCPP_INFO(nh_->get_logger(), "pose control algorithm");

  const geometry_msgs::msg::PoseStamped &finalgoalpose = plan_.back();
  const geometry_msgs::msg::PoseStamped &carrot_goalpose = plan_[currentPoseIndex_];
  const geometry_msgs::msg::Point &goalposition = carrot_goalpose.pose.position;

  tf2::Quaternion carrotGoalQ;
  tf2::fromMsg(carrot_goalpose.pose.orientation, carrotGoalQ);
  // RCLCPP_INFO_STREAM(nh_->get_logger(), "Plan goal quaternion at "<< carrot_goalpose.pose.orientation);

  // goal orientation (global frame)
  double betta = tf2::getYaw(carrot_goalpose.pose.orientation) + betta_offset_;
  double dx = goalposition.x - currentPose.pose.position.x;
  double dy = goalposition.y - currentPose.pose.position.y;

  // distance error to the targetpoint
  double rho_error = sqrt(dx * dx + dy * dy);

  tf2::Quaternion currentOrientation;
  tf2::fromMsg(currentPose.pose.orientation, currentOrientation);

  // current angle
  double theta = tf2::getYaw(currentOrientation);
  double alpha = atan2(dy, dx);
  alpha = alpha + alpha_offset_;

  double alpha_error = angles::shortest_angular_distance(alpha, theta);
  double betta_error = angles::shortest_angular_distance(betta, theta);

  double vetta = 0;  // = k_rho_ * rho_error;
  double gamma = 0;  //= k_alpha_ * alpha_error + k_betta_ * betta_error;

  if (rho_error > xy_goal_tolerance_)  // reguular control rule, be careful, rho error is with the carrot not with the
                                       // final goal (this is something to improve like the backwards planner)
  {
    vetta = k_rho_ * rho_error;
    gamma = k_alpha_ * alpha_error;
  }
  else if (fabs(betta_error) >= yaw_goal_tolerance_)  // pureSpining
  {
    vetta = 0;
    gamma = k_betta_ * betta_error;
  }
  else  // goal reached
  {
    RCLCPP_DEBUG(nh_->get_logger(), "GOAL REACHED");
    vetta = 0;
    gamma = 0;
    goalReached_ = true;
  }

  // linear speed clamp
  if (vetta > max_linear_x_speed_)
  {
    vetta = max_linear_x_speed_;
  }
  else if (vetta < -max_linear_x_speed_)
  {
    vetta = -max_linear_x_speed_;
  }

  // angular speed clamp
  if (gamma > max_angular_z_speed_)
  {
    gamma = max_angular_z_speed_;
  }
  else if (gamma < -max_angular_z_speed_)
  {
    gamma = -max_angular_z_speed_;
  }

  cmd_vel.twist.linear.x = vetta;
  cmd_vel.twist.angular.z = gamma;

  // clamp(cmd_vel, max_linear_x_speed_, max_angular_z_speed_);

  // RCLCPP_INFO_STREAM(nh_->get_logger(), "Local planner: "<< cmd_vel);

  publishGoalMarker(goalposition.x, goalposition.y, betta);

  RCLCPP_DEBUG_STREAM(nh_->get_logger(), "Forward local planner,"
                                             << std::endl
                                             << " theta: " << theta << std::endl
                                             << " betta: " << betta << std::endl
                                             << " err_x: " << dx << std::endl
                                             << " err_y:" << dy << std::endl
                                             << " rho_error:" << rho_error << std::endl
                                             << " alpha_error:" << alpha_error << std::endl
                                             << " betta_error:" << betta_error << std::endl
                                             << " vetta:" << vetta << std::endl
                                             << " gamma:" << gamma << std::endl
                                             << " xy_goal_tolerance:" << xy_goal_tolerance_ << std::endl
                                             << " yaw_goal_tolerance:" << yaw_goal_tolerance_ << std::endl);

  // if(cmd_vel.linear.x==0 && cmd_vel.angular.z == 0 )
  //{
  //}

  // integrate trajectory and check collision

  assert(currentPose.header.frame_id == "odom" || currentPose.header.frame_id == "map");
  auto global_pose = currentPose;
  //->getRobotPose(global_pose);

  auto *costmap2d = costmapRos_->getCostmap();
  auto yaw = tf2::getYaw(global_pose.pose.orientation);

  auto &pos = global_pose.pose.position;

  Eigen::Vector3f currentpose(pos.x, pos.y, yaw);
  Eigen::Vector3f currentvel(cmd_vel.twist.linear.x, cmd_vel.twist.linear.y, cmd_vel.twist.angular.z);

  std::vector<Eigen::Vector3f> trajectory;
  this->generateTrajectory(currentpose, currentvel, 0.8 /*meters*/, M_PI / 8 /*rads*/, 3.0 /*seconds*/,
                           0.05 /*seconds*/, trajectory);

  // check plan rejection
  bool aceptedplan = true;

  unsigned int mx, my;

  int i = 0;
  // RCLCPP_INFO_STREAM(nh_->get_logger(), "lplanner goal: " << finalgoalpose.pose.position);
  for (auto &p : trajectory)
  {
    float dx = p[0] - finalgoalpose.pose.position.x;
    float dy = p[1] - finalgoalpose.pose.position.y;

    float dst = sqrt(dx * dx + dy * dy);
    if (dst < xy_goal_tolerance_)
    {
      //  RCLCPP_INFO(nh_->get_logger(), "trajectory checking skipped, goal reached");
      break;
    }

    costmap2d->worldToMap(p[0], p[1], mx, my);

    // RCLCPP_INFO(nh_->get_logger(), "checking cost pt %d [%lf, %lf] cell[%d,%d] = %d", i, p[0], p[1], mx, my, cost);
    // RCLCPP_INFO_STREAM(nh_->get_logger(), "cost: " << cost);

    // static const unsigned char NO_INFORMATION = 255;
    // static const unsigned char LETHAL_OBSTACLE = 254;
    // static const unsigned char INSCRIBED_INFLATED_OBSTACLE = 253;
    // static const unsigned char FREE_SPACE = 0;

    if (costmap2d->getCost(mx, my) >= nav2_costmap_2d::INSCRIBED_INFLATED_OBSTACLE)
    {
      aceptedplan = false;
      // RCLCPP_WARN(nh_->get_logger(), "ABORTED LOCAL PLAN BECAUSE OBSTACLE DETEDTED");
      break;
    }
    i++;
  }

  bool success = false;
  if (aceptedplan)
  {
    waiting_ = false;
    success = true;
    RCLCPP_DEBUG(nh_->get_logger(), "simulated trajectory is accepted.");
  }
  else
  {
    RCLCPP_DEBUG(nh_->get_logger(), "simulated trajectory is not accepted. Stop command.");

    // stop and wait
    cmd_vel.twist.linear.x = 0;
    cmd_vel.twist.angular.z = 0;

    if (!waiting_)
    {
      RCLCPP_DEBUG(nh_->get_logger(), "Start waiting obstacle dissapear");
      waiting_ = true;
      waitingStamp_ = nh_->now();
    }
    else
    {
      auto waitingduration = nh_->now() - waitingStamp_;
      RCLCPP_DEBUG(nh_->get_logger(), "waiting obstacle dissapear, ellapsed: %lf seconds", waitingduration.seconds());

      if (waitingduration > this->waitingTimeout_)
      {
        RCLCPP_WARN(nh_->get_logger(), "TIMEOUT waiting obstacle dissapear, ellapsed: %lf seconds",
                    waitingduration.seconds());
        success = false;
      }
    }
  }

  if (!success)
  {
    RCLCPP_DEBUG(nh_->get_logger(), "[ForwardLocalPlanner] object detected waiting stopped until it disappears.");
  }

  cmd_vel.header.stamp = nh_->now();
  return cmd_vel;
}

void ForwardLocalPlanner::setSpeedLimit(const double &speed_limit, const bool &percentage)
{
    RCLCPP_WARN_STREAM(nh_->get_logger(), "ForwardLocalPlanner::setSpeedLimit invoked. Ignored, funcionality not implemented.");
}

/**
******************************************************************************************************************
* isGoalReached()
******************************************************************************************************************
*/
bool ForwardLocalPlanner::isGoalReached()
{
  return goalReached_;
}

/**
******************************************************************************************************************
* setPlan()
******************************************************************************************************************
*/
void ForwardLocalPlanner::setPlan(const nav_msgs::msg::Path &plan)
{
  plan_ = plan.poses;
  goalReached_ = false;
}
}  // namespace forward_local_planner
}  // namespace cl_move_base_z
PLUGINLIB_EXPORT_CLASS(cl_move_base_z::forward_local_planner::ForwardLocalPlanner, nav2_core::Controller)
