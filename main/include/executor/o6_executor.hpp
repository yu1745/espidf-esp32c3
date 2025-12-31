#pragma once

#include "executor/executor.hpp"
#include "geometry/o6_geometry.hpp"
#include "actuator/ledc_actuator.hpp"
#include "proto/setting.pb.h"
#include "utils.hpp"
#include <memory>
#include <array>

// LEDC resolution for servo control
#define O6_LEDC_RESOLUTION LEDC_TIMER_14_BIT

/**
 * @brief O6（6-axis parallel robot）executor class
 *
 * Inherits from Executor, used to control 6 servos for 6-axis parallel robot motion control
 * Uses LEDC peripheral for PWM output, implements 6-axis control via inverse kinematics
 */
class O6Executor : public Executor {
   public:
    /**
     * @brief Constructor
     * @param setting Configuration settings
     */
    explicit O6Executor(const SettingWrapper& setting);

    /**
     * @brief Destructor
     */
    ~O6Executor() override;

   protected:
    /**
     * @brief Calculate servo target positions
     * Parse L0-L2, R0-R2 values from tcode and calculate target angles for 6 servos using O6 kinematics
     */
    void compute() override;

    /**
     * @brief Execute servo control
     * Apply calculated duty cycles to all 6 servos
     */
    void execute() override;

   private:
    /**
     * @brief Initialize LEDC for all 6 servo channels
     * @return true on success, false on failure
     */
    bool initLEDC();

    // Servo actuators (6 channels for A-F)
    std::unique_ptr<actuator::LEDCActuator> m_servo_a;  // Servo A (channel 0)
    std::unique_ptr<actuator::LEDCActuator> m_servo_b;  // Servo B (channel 1)
    std::unique_ptr<actuator::LEDCActuator> m_servo_c;  // Servo C (channel 2)
    std::unique_ptr<actuator::LEDCActuator> m_servo_d;  // Servo D (channel 3)
    std::unique_ptr<actuator::LEDCActuator> m_servo_e;  // Servo E (channel 4)
    std::unique_ptr<actuator::LEDCActuator> m_servo_f;  // Servo F (channel 5)

    // Computed theta values (6 servo angles in radians)
    std::array<double, 6> m_theta_values;

    // Computed target values for servos (-1.0 to 1.0 range)
    float m_servo_a_target;
    float m_servo_b_target;
    float m_servo_c_target;
    float m_servo_d_target;
    float m_servo_e_target;
    float m_servo_f_target;

    // Initialization flag
    bool m_init_done = false;

    static const char* TAG;
    static constexpr int COMPUTE_TIMEOUT = 1000;  // Compute timeout threshold (microseconds)
};
