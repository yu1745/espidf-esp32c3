#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <tuple>
#include <vector>

// O6 offset constant (can be configured)
#ifndef O6_OFFSET
#define O6_OFFSET 0
#endif

namespace geometry {

using Point2D = std::array<double, 2>;
using Point3D = std::array<double, 3>;
using Point4D = std::array<double, 4>;
using Direction2D = std::array<double, 2>;
using Direction3D = std::array<double, 3>;
using Matrix4x4 = std::array<std::array<double, 4>, 4>;
using Matrix3x3 = std::array<std::array<double, 3>, 3>;

// Transform2D namespace - 2D transformation functions
namespace transform_2d {

/**
 * Convert 3-axis pose to 3x3 homogeneous transformation matrix
 * @param x, y: Position coordinates
 * @param theta: Rotation angle around z-axis (radians)
 * @return 3x3 homogeneous transformation matrix
 */
Matrix3x3 pose_to_homogeneous_matrix(double x, double y, double theta);

/**
 * Convert angle from degrees to radians
 * @param theta_deg: Angle in degrees
 * @return Angle in radians
 */
double degree_to_radian(double theta_deg);

/**
 * Transform point using homogeneous transformation matrix
 * @param T: 3x3 homogeneous transformation matrix
 * @param point: 2D point [x, y]
 * @return Transformed 2D point [x, y]
 */
Point2D transform_point(const Matrix3x3 &T, const Point2D &point);

/**
 * Transform homogeneous coordinate point using homogeneous transformation
 * matrix
 * @param T: 3x3 homogeneous transformation matrix
 * @param point: 3D homogeneous coordinate point [x, y, 1]
 * @return Transformed 3D homogeneous coordinate point
 */
std::array<double, 3>
transform_homogeneous_point(const Matrix3x3 &T,
                            const std::array<double, 3> &point);

} // namespace transform_2d

// Transform3D namespace - 3D transformation functions
namespace transform_3d {

/**
 * Convert 6-axis pose to 4x4 homogeneous transformation matrix
 * @param x, y, z: Position coordinates
 * @param roll: Rotation angle around x-axis (radians)
 * @param pitch: Rotation angle around y-axis (radians)
 * @param yaw: Rotation angle around z-axis (radians)
 * @return 4x4 homogeneous transformation matrix
 */
Matrix4x4 pose_to_homogeneous_matrix(double x, double y, double z, double roll,
                                     double pitch, double yaw);

/**
 * Convert Euler angles from degrees to radians
 * @param roll_deg, pitch_deg, yaw_deg: Angles in degrees
 * @return Tuple of angles in radians
 */
std::tuple<double, double, double>
euler_degrees_to_radians(double roll_deg, double pitch_deg, double yaw_deg);

/**
 * Transform point using homogeneous transformation matrix
 * @param T: 4x4 homogeneous transformation matrix
 * @param point: 3D point [x, y, z]
 * @return Transformed 3D point [x, y, z]
 */
Point3D transform_point(const Matrix4x4 &T, const Point3D &point);

/**
 * Transform homogeneous coordinate point using homogeneous transformation
 * matrix
 * @param T: 4x4 homogeneous transformation matrix
 * @param point: 4D homogeneous coordinate point [x, y, z, 1]
 * @return Transformed 4D homogeneous coordinate point
 */
Point4D transform_homogeneous_point(const Matrix4x4 &T, const Point4D &point);

} // namespace transform_3d

// Line class - 3D line in space
class Line {
public:
  /**
   * Constructor - Two-point form
   * @param point1: First point [x, y, z]
   * @param point2: Second point [x, y, z]
   */
  Line(const Point3D &point1, const Point3D &point2);

  /**
   * Constructor - Point-direction form
   * @param point: A point on the line [x, y, z]
   * @param direction: Direction vector [dx, dy, dz]
   * @param dummy: Dummy parameter to distinguish constructor overload
   */
  Line(const Point3D &point, const Direction3D &direction, int dummy);

  /**
   * Create a perpendicular line from an external point to this line
   * @param point: External point [x, y, z]
   * @return Line object representing the perpendicular line
   */
  Line vertical_line(const Point3D &point) const;

  /**
   * Calculate intersection point of two lines
   * @param other_line: Another Line object
   * @return Intersection point coordinates if exists, otherwise std::nullopt
   */
  std::optional<Point3D> intersection_with(const Line &other_line) const;

  /**
   * Rotate a point around this line
   * @param point: Point to rotate [x, y, z]
   * @param angle: Rotation angle (radians), positive for counter-clockwise
   * @return Rotated point [x, y, z]
   */
  Point3D rotate_point_around_line(const Point3D &point, double angle) const;

  /**
   * Get point on line at parameter t
   * @param t: Parameter value
   * @return Point on line [x, y, z]
   */
  Point3D get_point_at_parameter(double t) const;

  /**
   * Find closest point on this line to a given point
   * @param point: Point [x, y, z]
   * @return Closest point on line [x, y, z]
   */
  Point3D closest_point_on_line(const Point3D &point) const;

  /**
   * Calculate distance from point to this line
   * @param point: Point [x, y, z]
   * @return Distance from point to line
   */
  double distance_to_point(const Point3D &point) const;

  // Get reference point of line
  const Point3D &get_point() const { return point_; }

  // Get direction vector of line
  const Direction3D &get_direction() const { return direction_; }

private:
  Point3D point_;         // Reference point on line
  Direction3D direction_; // Unit direction vector
  Point3D point1_;        // First point (for two-point construction)
  Point3D point2_;        // Second point (for two-point construction)

  // Initialize from two points
  void init_from_two_points(const Point3D &point1, const Point3D &point2);

  // Initialize from point and direction vector
  void init_from_point_direction(const Point3D &point,
                                 const Direction3D &direction);
};

/**
 * Five-bar linkage inverse kinematics solver
 * @param motor1: Motor1 position [x, y, z]
 * @param motor2: Motor2 position [x, y, z]
 * @param arm: Rocker arm length
 * @param link: Connecting rod length
 * @param p: Output endpoint position [x, y, z]
 * @return tuple: (theta1, theta2) Two rocker angles, when solution exists take
 * y>0 solution std::nullopt: No solution
 */
std::optional<std::tuple<double, double>>
five_bar_back_kinematics(const Point3D &motor1, const Point3D &motor2,
                         double arm, double link, const Point3D &p);

// Utility functions
namespace utils {

/**
 * Calculate distance between two points
 * @param p1: First point
 * @param p2: Second point
 * @return Distance between two points
 */
double distance(const Point3D &p1, const Point3D &p2);

/**
 * Calculate distance between two points (2D version)
 * @param p1: First point
 * @param p2: Second point
 * @return Distance between two points
 */
double distance(const Point2D &p1, const Point2D &p2);

/**
 * Calculate vector norm
 * @param v: Vector
 * @return Vector norm
 */
double norm(const Direction3D &v);

/**
 * Calculate vector norm (2D version)
 * @param v: Vector
 * @return Vector norm
 */
double norm(const Direction2D &v);

/**
 * Calculate dot product of two vectors
 * @param v1: First vector
 * @param v2: Second vector
 * @return Dot product
 */
double dot(const Point3D &v1, const Point3D &v2);

/**
 * Calculate dot product of two vectors (2D version)
 * @param v1: First vector
 * @param v2: Second vector
 * @return Dot product
 */
double dot(const Direction2D &v1, const Direction2D &v2);

/**
 * Calculate cross product of two vectors
 * @param v1: First vector
 * @param v2: Second vector
 * @return Cross product
 */
Point3D cross(const Point3D &v1, const Point3D &v2);

/**
 * Calculate cross product of two 2D vectors (scalar)
 * @param v1: First vector
 * @param v2: Second vector
 * @return Cross product (scalar, representing perpendicular component)
 */
double cross(const Direction2D &v1, const Direction2D &v2);

/**
 * Calculate angle between two 3D direction vectors
 * @param v1: First direction vector
 * @param v2: Second direction vector
 * @return Angle between two vectors (radians)
 */
double angle_between_directions(const Direction3D &v1, const Direction3D &v2);

/**
 * Calculate angle between two 2D direction vectors
 * @param v1: First direction vector
 * @param v2: Second direction vector
 * @return Angle between two vectors (radians)
 */
double angle_between_directions(const Direction2D &v1, const Direction2D &v2);

/**
 * Matrix multiplication
 * @param m1: First 4x4 matrix
 * @param m2: Second 4x4 matrix
 * @return Matrix product
 */
Matrix4x4 matrix_multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);

/**
 * Matrix-vector multiplication
 * @param m: 4x4 matrix
 * @param v: 4D vector
 * @return Product vector
 */
Point4D matrix_vector_multiply(const Matrix4x4 &m, const Point4D &v);

/**
 * 3x3 matrix multiplication
 * @param m1: First 3x3 matrix
 * @param m2: Second 3x3 matrix
 * @return Matrix product
 */
Matrix3x3 matrix_multiply(const Matrix3x3 &m1, const Matrix3x3 &m2);

/**
 * 3x3 matrix-vector multiplication
 * @param m: 3x3 matrix
 * @param v: 3D vector
 * @return Product vector
 */
std::array<double, 3> matrix_vector_multiply(const Matrix3x3 &m,
                                             const std::array<double, 3> &v);

} // namespace utils

// Point3D operator overloading
Point3D operator+(const Point3D &lhs, const Point3D &rhs);
Point3D operator-(const Point3D &lhs, const Point3D &rhs);
Point3D operator*(const Point3D &lhs, double scalar);
Point3D operator*(double scalar, const Point3D &rhs);
Point3D operator/(const Point3D &lhs, double scalar);

// Direction2D operator overloading
Direction2D operator+(const Direction2D &lhs, const Direction2D &rhs);
Direction2D operator-(const Direction2D &lhs, const Direction2D &rhs);
Direction2D operator*(const Direction2D &lhs, double scalar);
Direction2D operator*(double scalar, const Direction2D &rhs);
Direction2D operator/(const Direction2D &lhs, double scalar);

/**
 * Robot kinematics solver function
 * @param x, y, z: Position coordinates
 * @param roll_deg, pitch_deg, yaw_deg: Attitude angles (degrees)
 * @return Array of 6 theta angles if solution exists, otherwise std::nullopt
 */
std::optional<std::array<double, 6>>
solve_robot_kinematics(double x, double y, double z, double roll_deg,
                       double pitch_deg, double yaw_deg, double r = 4.9,
                       double arm = 9.0, double link = 21.0,
                       Point3D a = {7.8, -1.25, 0}, Point3D b = {7.8, 1.25, 0});

} // namespace geometry
