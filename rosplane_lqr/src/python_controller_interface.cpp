#include <cmath>

#include "python_controller_interface.hpp"

namespace rosplane
{

double wrap_within_180(double fixed_heading, double wrapped_heading)
{
  // wrapped_heading - number_of_times_to_wrap * 2pi
  return wrapped_heading - floor((wrapped_heading - fixed_heading) / (2 * M_PI) + 0.5) * 2 * M_PI;
}

PythonControllerInterface::PythonControllerInterface()
{
  // Declare parameters associated with this controller, controller_state_machine
  declare_parameters();
  // Set parameters according to the parameters in the launch file, otherwise use the default values
  params_.set_parameters();
}

void PythonControllerInterface::take_off(const Input & input, Output & output)
{
  // Run lateral and longitudinal controls.
  take_off_lateral_control(input, output);
  take_off_longitudinal_control(input, output);
}

void PythonControllerInterface::take_off_exit()
{
  // Put any code that should run as the airplane exits take off mode.
}

void PythonControllerInterface::climb(const Input & input, Output & output)
{
  // Run lateral and longitudinal controls.
  climb_lateral_control(input, output);
  climb_longitudinal_control(input, output);
}

void PythonControllerInterface::climb_exit()
{
  // Put any code that should run as the airplane exits take off mode.
}

void PythonControllerInterface::altitude_hold(const Input & input, Output & output)
{
  // Run lateral and longitudinal controls.
  alt_hold_lateral_control(input, output);
  alt_hold_longitudinal_control(input, output);
}

void PythonControllerInterface::altitude_hold_exit()
{

}

void PythonControllerInterface::alt_hold_lateral_control(const Input & input, Output & output)
{

}

void PythonControllerInterface::alt_hold_longitudinal_control(const Input & input, Output & output)
{
  
}

void PythonControllerInterface::climb_lateral_control(const Input & input, Output & output)
{

}

void PythonControllerInterface::climb_longitudinal_control(const Input & input, Output & output)
{

}

void PythonControllerInterface::take_off_lateral_control(const Input & input, Output & output)
{

}

void PythonControllerInterface::take_off_longitudinal_control(const Input & input, Output & output)
{

}
  
void PythonControllerInterface::lqr_lateral_control(const Input & input, Output & output)
{

}

void PythonControllerInterface::lqr_longitudinal_control(const Input & input, Output & output)
{

}

float PythonControllerInterface::sat(float value, float up_limit, float low_limit)
{
  // Set to upper limit if larger than that limit.
  // Set to lower limit if smaller than that limit.
  // Otherwise, do not change the value.

  if (up_limit < 0.0) {
    RCLCPP_WARN_ONCE(this->get_logger(),
                     "WARNING: Upper limit in saturation function is negative.");
  }

  float rVal;
  if (value > up_limit)
    rVal = up_limit;
  else if (value < low_limit)
    rVal = low_limit;
  else
    rVal = value;

  // Return the saturated value.
  return rVal;
}

float PythonControllerInterface::adjust_h_c(float h_c, float h, float max_diff)
{
  double adjusted_h_c;

  // If the error in altitude is larger than the max altitude, adjust it to the max with the correct sign.
  // Otherwise, proceed as normal.
  if (abs(h_c - h) > max_diff) {
    adjusted_h_c = h + copysign(max_diff, h_c - h);
  } else {
    adjusted_h_c = h_c;
  }

  return adjusted_h_c;
}

void PythonControllerInterface::declare_parameters()
{
  // Declare param with ROS2 and set the default value.
}

} // namespace rosplane
