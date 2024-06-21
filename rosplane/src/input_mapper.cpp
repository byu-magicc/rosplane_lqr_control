#include "input_mapper.hpp"

using std::placeholders::_1;

namespace rosplane
{

input_mapper::input_mapper() :
  Node("input_mapper"),
  roll_override_(false),
  pitch_override_(false),
  param_change_pending_(false),
  params_(this)
{
  mixed_commands_pub_ = this->create_publisher<rosplane_msgs::msg::ControllerCommands>(
    "mixed_commands", 10);

  controller_commands_sub_ = this->create_subscription<rosplane_msgs::msg::ControllerCommands>(
    "controller_commands", 10,
    std::bind(&input_mapper::controller_commands_callback, this, _1));
  rc_raw_sub_ = this->create_subscription<rosflight_msgs::msg::RCRaw>(
      "rc_raw", 10,
      std::bind(&input_mapper::rc_raw_callback, this, _1));
  state_sub_ = this->create_subscription<rosplane_msgs::msg::State>(
      "estimated_state", 10,
      std::bind(&input_mapper::state_callback, this, _1));

  set_param_client_ = this->create_client<rcl_interfaces::srv::SetParameters>(
    "/autopilot/set_parameters", rmw_qos_profile_services_default);
  set_param_timer_ = this->create_wall_timer(
    std::chrono::milliseconds(100), std::bind(&input_mapper::set_param_timer_callback, this));
  set_param_timer_->cancel();

  last_command_time_ = this->now();
  mixed_commands_msg_ = std::make_shared<rosplane_msgs::msg::ControllerCommands>();
  rc_raw_msg_ = std::make_shared<rosflight_msgs::msg::RCRaw>();
  state_msg_ = std::make_shared<rosplane_msgs::msg::State>();

  /// Parameters stuff

  params_.declare_string("aileron_input", "path_follower");
  params_.declare_string("elevator_input", "path_follower");
  params_.declare_string("throttle_input", "path_follower");
  params_.declare_double("rc_roll_angle_min_max", 0.5);
  params_.declare_double("rc_course_rate", 0.5);
  params_.declare_double("rc_pitch_angle_min_max", 0.5);
  params_.declare_double("rc_altitude_rate", 3.0);
  params_.declare_double("rc_airspeed_rate", 1.0);

  // Set the parameter callback, for when parameters are changed.
  parameter_callback_handle_ = this->add_on_set_parameters_callback(
    std::bind(&input_mapper::parametersCallback, this, _1));
}

void input_mapper::set_param_timer_callback()
{
  // Check that the service is ready
  if (!set_param_client_->service_is_ready()) {
    RCLCPP_INFO(this->get_logger(), "/autopilot/set_parameters service not available, waiting...");
    return;
  }

  // Send the request
  if (set_param_request_ != nullptr) {
    set_param_future_ = set_param_client_->async_send_request(set_param_request_).share();
    set_param_request_ = nullptr;
    return;
  }

  // Wait for the response
  if (set_param_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return;
  }

  // Check that the parameter was set successfully
  for (auto & result : set_param_future_.get()->results) {
    if (!result.successful) {
      RCLCPP_ERROR(this->get_logger(), "Failed to set parameter: %s", result.reason.c_str());
    }
  }

  set_param_timer_->cancel();
  param_change_pending_ = false;
}

void input_mapper::set_roll_override(bool roll_override)
{
  // Value hasn't changed, return
  if (roll_override == roll_override_) {
    return;
  }
  // Previous parameter request is still pending, return
  if (param_change_pending_) {
    return;
  }
  roll_override_ = roll_override;

  // Contruct the parameter request object
  auto parameter = rcl_interfaces::msg::Parameter();
  parameter.name = "roll_command_override";
  parameter.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_BOOL;
  parameter.value.bool_value = roll_override;
  set_param_request_ = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
  set_param_request_->parameters.push_back(parameter);
  set_param_timer_->reset();
  param_change_pending_ = true;
}

void input_mapper::set_pitch_override(bool pitch_override)
{
  // Value hasn't changed, return
  if (pitch_override == pitch_override_) {
    return;
  }
  // Previous parameter request is still pending, return
  if (param_change_pending_) {
    return;
  }
  pitch_override_ = pitch_override;

  // Contruct the parameter request object
  auto parameter = rcl_interfaces::msg::Parameter();
  parameter.name = "pitch_command_override";
  parameter.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_BOOL;
  parameter.value.bool_value = pitch_override;
  set_param_request_ = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
  set_param_request_->parameters.push_back(parameter);
  set_param_timer_->reset();
  param_change_pending_ = true;
}

void input_mapper::controller_commands_callback(const rosplane_msgs::msg::ControllerCommands::SharedPtr msg)
{
  std::string aileron_input = params_.get_string("aileron_input");
  std::string elevator_input = params_.get_string("elevator_input");
  std::string throttle_input = params_.get_string("throttle_input");
  double elapsed_time = (this->now() - last_command_time_).seconds();
  last_command_time_ = this->now();

  if (aileron_input == "path_follower") {
    set_roll_override(false);
    mixed_commands_msg_->chi_c = msg->chi_c;
  } else if (aileron_input == "rc_course") {
    set_roll_override(false);
    double norm_aileron = (rc_raw_msg_->values[0] - 1500) / 500.0;
    mixed_commands_msg_->chi_c += norm_aileron *
                                  params_.get_double("rc_course_rate") *
                                  elapsed_time;
  } else if (aileron_input == "rc_roll_angle") {
    set_roll_override(true);
    double norm_aileron = (rc_raw_msg_->values[0] - 1500) / 500.0;
    mixed_commands_msg_->phi_c = norm_aileron * params_.get_double("rc_roll_angle_min_max");
    mixed_commands_msg_->chi_c = state_msg_->chi;
  } else {
    RCLCPP_ERROR(this->get_logger(), "Invalid aileron input type: %s. Valid options are "
                                     "path_follower, rc_course, and rc_roll_angle. Setting to "
                                     "path_follower.",
                 aileron_input.c_str());
    params_.set_string("aileron_input", "path_follower");
  }

  if (elevator_input == "path_follower") {
    set_pitch_override(false);
    mixed_commands_msg_->h_c = msg->h_c;
  } else if (elevator_input == "rc_altitude") {
    set_pitch_override(false);
    double norm_elevator = (rc_raw_msg_->values[1] - 1500) / 500.0;
    mixed_commands_msg_->h_c += norm_elevator *
                                params_.get_double("rc_altitude_rate") *
                                elapsed_time;
  } else if (elevator_input == "rc_pitch_angle") {
    set_pitch_override(true);
    double norm_elevator = (rc_raw_msg_->values[1] - 1500) / 500.0;
    mixed_commands_msg_->theta_c = norm_elevator * params_.get_double("rc_pitch_angle_min_max");
    mixed_commands_msg_->h_c = -state_msg_->position[2];
  } else {
    RCLCPP_ERROR(this->get_logger(), "Invalid elevator input type: %s. Valid options are "
                                     "path_follower, rc_altitude, and rc_pitch_angle. Setting to "
                                     "path_follower.",
                 elevator_input.c_str());
    params_.set_string("elevator_input", "path_follower");
  }

  if (throttle_input == "path_follower") {
    mixed_commands_msg_->va_c = msg->va_c;
  } else if (throttle_input == "rc_airspeed") {
    double norm_throttle = (rc_raw_msg_->values[2] - 1500) / 500.0;
    mixed_commands_msg_->va_c += norm_throttle *
                                 params_.get_double("rc_airspeed_rate") *
                                 elapsed_time;
  } else {
    RCLCPP_ERROR(this->get_logger(), "Invalid throttle input type: %s. Valid options are "
                                     "path_follower and rc_airspeed. Setting to path_follower.",
                 throttle_input.c_str());
    params_.set_string("throttle_input", "path_follower");
  }

  mixed_commands_pub_->publish(*mixed_commands_msg_);
}

void input_mapper::rc_raw_callback(const rosflight_msgs::msg::RCRaw::SharedPtr msg)
{
  rc_raw_msg_ = msg;
}

void input_mapper::state_callback(const rosplane_msgs::msg::State::SharedPtr msg)
{
  state_msg_ = msg;
}

rcl_interfaces::msg::SetParametersResult
input_mapper::parametersCallback(const std::vector<rclcpp::Parameter> & parameters)
{
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;
  result.reason = "success";

  bool success = params_.set_parameters_callback(parameters);
  if (!success)
  {
    result.successful = false;
    result.reason =
      "One of the parameters given does not is not a parameter of the input mapper node.";
  }

  return result;
}
} // namespace rosplane

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rosplane::input_mapper>();
  rclcpp::spin(node);
  return 0;
}