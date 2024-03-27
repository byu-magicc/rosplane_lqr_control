#include "controller_total_energy.hpp"

namespace rosplane
{

controller_total_energy::controller_total_energy()
    : controller_successive_loop()
{
  // Initialize course hold, roll hold and pitch hold errors and integrators to zero.
  L_integrator_ = 0;
  E_integrator_ = 0;

  // Declare parameters associated with this controller, controller_state_machine
  declare_parameters();
  // Set parameters according to the parameters in the launch file, otherwise use the default values
  params.set_parameters();
}

void controller_total_energy::take_off_longitudinal_control(const struct input_s & input,
                                                            struct output_s & output)
{
  // For readability, declare parameters here that will be used in this function
  double max_takeoff_throttle = params.get_double("max_takeoff_throttle");
  double cmd_takeoff_pitch = params.get_double("cmd_takeoff_pitch");   // Declared in controller_successive_loop

  // Set throttle to not overshoot altitude.
  output.delta_t = sat(total_energy_throttle(input.Va_c, input.va, input.h_c, input.h),
                       max_takeoff_throttle, 0);

  // Command a shallow pitch angle to gain altitude.
  output.theta_c = cmd_takeoff_pitch * 3.14 / 180.0;
  output.delta_e = pitch_hold(output.theta_c, input.theta, input.q);
}

void controller_total_energy::take_off_exit()
{
  // Run parent exit code.
  controller_successive_loop::take_off_exit();

  L_integrator_ = 0;
  E_integrator_ = 0;

  // Place any controller code that should run as you exit the take-off regime here.
}

void controller_total_energy::climb_longitudinal_control(const struct input_s & input,
                                                         struct output_s & output)
{
  // For readability, declare parameters here that will be used in this function
  double alt_hz = params.get_double("alt_hz");

  double adjusted_hc = adjust_h_c(input.h_c, input.h, alt_hz / 2.0);
  // Find the control efforts for throttle and find the commanded pitch angle using total energy.
  output.delta_t = total_energy_throttle(input.Va_c, input.va, adjusted_hc, input.h);
  output.theta_c = total_energy_pitch(input.Va_c, input.va, adjusted_hc, input.h);
  output.delta_e = pitch_hold(output.theta_c, input.theta, input.q);
}

void controller_total_energy::climb_exit()
{
  //Run parent exit code
  controller_successive_loop::climb_exit();

  L_integrator_ = 0;
  E_integrator_ = 0;

  // Place any controller code that should run as you exit the climb regime here.
}

void controller_total_energy::alt_hold_longitudinal_control(const struct input_s & input,
                                                            struct output_s & output)
{
  // For readability, declare parameters here that will be used in this function
  double alt_hz = params.get_double("alt_hz");

  // Saturate altitude command.
  double adjusted_hc = adjust_h_c(input.h_c, input.h, alt_hz);

  // Calculate the control effort to maintain airspeed and the required pitch angle to maintain altitude.
  output.delta_t = total_energy_throttle(input.Va_c, input.va, adjusted_hc, input.h);
  output.theta_c = total_energy_pitch(input.Va_c, input.va, adjusted_hc, input.h); // TODO remove capital from Va_c
  output.delta_e = pitch_hold(output.theta_c, input.theta, input.q);
}

void controller_total_energy::altitude_hold_exit()
{
  // Run parent exit code.
  controller_successive_loop::altitude_hold_exit();

  // Reset the integrators in the event of returning to the take-off regime (likely a crash).
  L_integrator_ = 0;
  E_integrator_ = 0;
}

float controller_total_energy::total_energy_throttle(float va_c, float va, float h_c, float h)
{
  // For readability, declare parameters here that will be used in this function
  int64_t frequency = params.get_double("frequency");
  double e_kp = params.get_double("e_kp");
  double e_ki = params.get_double("e_ki");
  double e_kd = params.get_double("e_kd");
  double max_t = params.get_double("max_t");   // Declared in controller_successive_loop
  double trim_t = params.get_double("trim_t");   // Declared in controller_successive_loop

  // Update energies based off of most recent data.
  update_energies(va_c, va, h_c, h);

  // Calculate total energy error, and normalize relative to the desired kinetic energy.
  float E_error = (K_error + U_error) / K_ref;

  float Ts = 1.0 / frequency;

  // Integrate error.
  E_integrator_ = E_integrator_ + (Ts / 2.0) * (E_error + E_error_prev_);

  E_error_prev_ = E_error;

  if (h < .5) { E_integrator_ = 0; }

  // Return saturated throttle command.
  return sat(e_kp * E_error + e_ki * E_integrator_, max_t, 0.0)
    + trim_t;
}

float controller_total_energy::total_energy_pitch(float va_c, float va, float h_c, float h)
{
  // For readability, declare parameters here that will be used in this function
  int64_t frequency = params.get_double("frequency");
  double l_kp = params.get_double("l_kp");
  double l_ki = params.get_double("l_ki");
  double l_kd = params.get_double("l_kd");
  double max_roll = params.get_double("max_roll");   // Declared in controller_successive_loop

  // Update energies based off of most recent data.
  update_energies(va_c, va, h_c, h);

  // Calculate energy balance error, and normalize relative to the desired kinetic energy.

  float L_error = (U_error - K_error) / K_ref;

  float Ts = 1.0 / frequency;

  // Integrate error.
  L_integrator_ = L_integrator_ + (Ts / 2.0) * (L_error + L_error_prev_);

  L_error_prev_ = L_error;

  // Return saturated pitch command.
  return sat(l_kp * L_error + l_ki * L_integrator_, max_roll * M_PI / 180.0,
             -max_roll * M_PI / 180.0); 
}

void controller_total_energy::update_energies(float va_c, float va, float h_c, float h)
{
  // For readability, declare parameters here that will be used in this function
  double mass = params.get_double("mass");
  double gravity = params.get_double("gravity");
  double max_energy = params.get_double("max_energy");

  // Calculate the error in kinetic energy.
  K_error = 0.5 * mass * (pow(va_c, 2) - pow(va, 2));

  // Clacualte the desired kinetic energy.
  K_ref = 0.5 * mass * pow(va_c, 2);

  // Calculate the error in the potential energy.
  U_error = mass * gravity * sat(h_c - h, max_energy, -max_energy);
}

void controller_total_energy::declare_parameters()
{
  // Declare parameter with ROS2 and set the default value
  params.declare_param("e_kp", 5.0);
  params.declare_param("e_ki", 0.9);
  params.declare_param("e_kd", 0.0);

  params.declare_param("l_kp", 1.0);
  params.declare_param("l_ki", 0.05);
  params.declare_param("l_kd", 0.0);

  params.declare_param("mass", 2.28);
  params.declare_param("gravity", 9.8);
  params.declare_param("max_energy", 5.0);
}
} // namespace rosplane
