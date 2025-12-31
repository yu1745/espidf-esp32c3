#include "geometry/o6_geometry.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "esp_log.h"

namespace geometry {

namespace transform_2d {

Matrix3x3 pose_to_homogeneous_matrix(double x, double y, double theta) {
    // Calculate rotation matrix components
    double cos_theta = std::cos(theta);
    double sin_theta = std::sin(theta);

    // Build 3x3 homogeneous transformation matrix
    Matrix3x3 T = {{{{cos_theta, -sin_theta, x}},
                    {{sin_theta, cos_theta, y}},
                    {{0.0, 0.0, 1.0}}}};

    return T;
}

double degree_to_radian(double theta_deg) {
    return theta_deg * M_PI / 180.0;
}

Point2D transform_point(const Matrix3x3& T, const Point2D& point) {
    // Convert 2D point to homogeneous coordinates
    std::array<double, 3> homogeneous_point = {{point[0], point[1], 1.0}};

    // Perform matrix transformation
    std::array<double, 3> transformed =
        transform_homogeneous_point(T, homogeneous_point);

    // Convert back to 2D point
    return {{transformed[0], transformed[1]}};
}

std::array<double, 3> transform_homogeneous_point(
    const Matrix3x3& T,
    const std::array<double, 3>& point) {
    return utils::matrix_vector_multiply(T, point);
}

}  // namespace transform_2d

namespace transform_3d {

Matrix4x4 pose_to_homogeneous_matrix(double x,
                                     double y,
                                     double z,
                                     double roll,
                                     double pitch,
                                     double yaw) {
    // Calculate rotation matrix components
    double cos_roll = std::cos(roll);
    double sin_roll = std::sin(roll);
    double cos_pitch = std::cos(pitch);
    double sin_pitch = std::sin(pitch);
    double cos_yaw = std::cos(yaw);
    double sin_yaw = std::sin(yaw);

    // Build rotation matrix (ZYX Euler angle order)
    double R11 = cos_yaw * cos_pitch;
    double R12 = cos_yaw * sin_pitch * sin_roll - sin_yaw * cos_roll;
    double R13 = cos_yaw * sin_pitch * cos_roll + sin_yaw * sin_roll;

    double R21 = sin_yaw * cos_pitch;
    double R22 = sin_yaw * sin_pitch * sin_roll + cos_yaw * cos_roll;
    double R23 = sin_yaw * sin_pitch * cos_roll - cos_yaw * sin_roll;

    double R31 = -sin_pitch;
    double R32 = cos_pitch * sin_roll;
    double R33 = cos_pitch * cos_roll;

    // Build 4x4 homogeneous transformation matrix
    Matrix4x4 T = {{{{R11, R12, R13, x}},
                    {{R21, R22, R23, y}},
                    {{R31, R32, R33, z}},
                    {{0.0, 0.0, 0.0, 1.0}}}};

    return T;
}

std::tuple<double, double, double> euler_degrees_to_radians(double roll_deg,
                                                            double pitch_deg,
                                                            double yaw_deg) {
    return std::make_tuple(roll_deg * M_PI / 180.0, pitch_deg * M_PI / 180.0,
                           yaw_deg * M_PI / 180.0);
}

Point3D transform_point(const Matrix4x4& T, const Point3D& point) {
    // Convert 3D point to homogeneous coordinates
    Point4D homogeneous_point = {{point[0], point[1], point[2], 1.0}};

    // Perform matrix transformation
    Point4D transformed = transform_homogeneous_point(T, homogeneous_point);

    // Convert back to 3D point
    return {{transformed[0], transformed[1], transformed[2]}};
}

Point4D transform_homogeneous_point(const Matrix4x4& T, const Point4D& point) {
    return utils::matrix_vector_multiply(T, point);
}

}  // namespace transform_3d

// Line class implementation
Line::Line(const Point3D& point1, const Point3D& point2) {
    init_from_two_points(point1, point2);
}

Line::Line(const Point3D& point, const Direction3D& direction, int dummy) {
    init_from_point_direction(point, direction);
}

void Line::init_from_two_points(const Point3D& point1, const Point3D& point2) {
    point1_ = point1;
    point2_ = point2;

    // Check if two points are the same
    if (utils::distance(point1, point2) <
        std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Two points cannot be the same");
    }

    // Calculate direction vector
    direction_ = {
        {point2[0] - point1[0], point2[1] - point1[1], point2[2] - point1[2]}};

    // Normalize direction vector
    double norm = utils::norm(direction_);
    if (norm < std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Direction vector cannot be zero");
    }

    for (int i = 0; i < 3; ++i) {
        direction_[i] /= norm;
    }

    // Select first point as reference point
    point_ = point1;
}

void Line::init_from_point_direction(const Point3D& point,
                                     const Direction3D& direction) {
    point_ = point;
    direction_ = direction;

    // Check if direction vector is zero
    double norm = utils::norm(direction);
    if (norm < std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Direction vector cannot be zero");
    }

    // Normalize direction vector
    for (int i = 0; i < 3; ++i) {
        direction_[i] /= norm;
    }

    // For consistency, calculate second point (for visualization)
    point1_ = point;
    point2_ = {{point[0] + direction_[0], point[1] + direction_[1],
                point[2] + direction_[2]}};
}

Line Line::vertical_line(const Point3D& point) const {
    // Find closest point on line to given point (foot of perpendicular)
    Point3D foot_point = closest_point_on_line(point);

    // Perpendicular line direction is from given point to foot point
    Point3D perpendicular_direction = {{foot_point[0] - point[0],
                                        foot_point[1] - point[1],
                                        foot_point[2] - point[2]}};

    // Check if given point is on the line
    if (utils::norm(perpendicular_direction) <
        std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Given point is on the line");
    }

    // Create and return perpendicular line
    return Line(point, perpendicular_direction, 0);
}

std::optional<Point3D> Line::intersection_with(const Line& other_line) const {
    const double tolerance = 1e-6;

    // Check if parallel
    Point3D cross_product = utils::cross(direction_, other_line.direction_);
    if (utils::norm(cross_product) < tolerance) {
        // Check if coincident
        double distance = distance_to_point(other_line.point_);
        if (distance < tolerance) {
            // Coincident, return any point on first line
            return point_;
        } else {
            // Parallel but not coincident, no intersection
            return std::nullopt;
        }
    }

    // Solve linear equations for intersection point
    // P1 + t1 * d1 = P2 + t2 * d2
    // Convert to matrix form: [d1, -d2] * [t1, t2]^T = P2 - P1

    // Build coefficient matrix and right-hand side vector
    double a11 = direction_[0];
    double a12 = -other_line.direction_[0];
    double a21 = direction_[1];
    double a22 = -other_line.direction_[1];
    double a31 = direction_[2];
    double a32 = -other_line.direction_[2];

    double b1 = other_line.point_[0] - point_[0];
    double b2 = other_line.point_[1] - point_[1];
    double b3 = other_line.point_[2] - point_[2];

    // Calculate determinants
    double det1 = a11 * a22 - a12 * a21;
    double det2 = a11 * a32 - a12 * a31;
    double det3 = a21 * a32 - a22 * a31;

    // Choose two equations with largest determinant to solve
    if (std::abs(det1) >= std::abs(det2) && std::abs(det1) >= std::abs(det3)) {
        // Use first two equations
        if (std::abs(det1) < tolerance) {
            return std::nullopt;
        }

        double t1 = (b1 * a22 - a12 * b2) / det1;
        Point3D intersection = {{point_[0] + t1 * direction_[0],
                                 point_[1] + t1 * direction_[1],
                                 point_[2] + t1 * direction_[2]}};
        return intersection;
    } else if (std::abs(det2) >= std::abs(det3)) {
        // Use first and third equations
        if (std::abs(det2) < tolerance) {
            return std::nullopt;
        }

        double t1 = (b1 * a32 - a12 * b3) / det2;
        Point3D intersection = {{point_[0] + t1 * direction_[0],
                                 point_[1] + t1 * direction_[1],
                                 point_[2] + t1 * direction_[2]}};
        return intersection;
    } else {
        // Use second and third equations
        if (std::abs(det3) < tolerance) {
            return std::nullopt;
        }

        double t1 = (b2 * a32 - a22 * b3) / det3;
        Point3D intersection = {{point_[0] + t1 * direction_[0],
                                 point_[1] + t1 * direction_[1],
                                 point_[2] + t1 * direction_[2]}};
        return intersection;
    }
}

Point3D Line::rotate_point_around_line(const Point3D& point,
                                       double angle) const {
    // 1. Find projection point of point on line
    Point3D projection_point = closest_point_on_line(point);

    // 2. Calculate vector from projection point to original point
    Point3D r = {{point[0] - projection_point[0],
                  point[1] - projection_point[1],
                  point[2] - projection_point[2]}};

    // 3. If point is on line, position unchanged after rotation
    if (utils::norm(r) < std::numeric_limits<double>::epsilon()) {
        return point;
    }

    // 4. Use Rodrigues' rotation formula
    // r_rotated = r*cos(θ) + (k×r)*sin(θ) + k*(k·r)*(1-cos(θ))
    // where k is unit direction vector, θ is rotation angle
    const Point3D& k = direction_;
    double cos_angle = std::cos(angle);
    double sin_angle = std::sin(angle);

    // Calculate cross product k×r
    Point3D cross_product = utils::cross(k, r);

    // Calculate dot product k·r
    double dot_product = utils::dot(k, r);

    // Apply Rodrigues formula
    Point3D r_rotated = {{r[0] * cos_angle + cross_product[0] * sin_angle +
                              k[0] * dot_product * (1 - cos_angle),
                          r[1] * cos_angle + cross_product[1] * sin_angle +
                              k[1] * dot_product * (1 - cos_angle),
                          r[2] * cos_angle + cross_product[2] * sin_angle +
                              k[2] * dot_product * (1 - cos_angle)}};

    // 5. Calculate rotated point
    Point3D rotated_point = {{projection_point[0] + r_rotated[0],
                              projection_point[1] + r_rotated[1],
                              projection_point[2] + r_rotated[2]}};

    return rotated_point;
}

Point3D Line::get_point_at_parameter(double t) const {
    return {{point_[0] + t * direction_[0], point_[1] + t * direction_[1],
             point_[2] + t * direction_[2]}};
}

Point3D Line::closest_point_on_line(const Point3D& point) const {
    // Vector P0P
    Point3D vec_p0p = {
        {point[0] - point_[0], point[1] - point_[1], point[2] - point_[2]}};

    // Project onto direction vector
    double projection_length = utils::dot(vec_p0p, direction_);

    // Closest point
    Point3D closest_point = {{point_[0] + projection_length * direction_[0],
                              point_[1] + projection_length * direction_[1],
                              point_[2] + projection_length * direction_[2]}};

    return closest_point;
}

// Five-bar linkage inverse kinematics solver implementation
std::optional<std::tuple<double, double>> five_bar_back_kinematics(
    const Point3D& motor1,
    const Point3D& motor2,
    double arm,
    double link,
    const Point3D& p) {
    // Validate parameters
    if (arm <= 0 || link <= 0) {
        throw std::invalid_argument("Rocker arm and connecting rod lengths must be greater than 0");
    }
    try {
        Point3D mid_point = (motor1 + motor2) / 2;
        double a = link;
        double b = arm;
        double c = utils::distance(p, motor1);
        double d = utils::norm(p - mid_point);
        double e = utils::norm(mid_point - motor1);
        double theta1 = std::acos((b * b + c * c - a * a) / (2 * b * c));
        double theta2 = std::acos((e * e + c * c - d * d) / (2 * e * c));
        double res1 = M_PI - theta1 - theta2;

        double a1 = link;
        double b1 = arm;
        double c1 = utils::distance(p, motor2);
        double d1 = utils::norm(p - mid_point);
        double e1 = utils::norm(mid_point - motor2);

        double theta3 = std::acos((b1 * b1 + c1 * c1 - a1 * a1) / (2 * b1 * c1));
        double theta4 = std::acos((e1 * e1 + c1 * c1 - d1 * d1) / (2 * e1 * c1));
        double res2 = M_PI - theta3 - theta4;

        return std::make_tuple(res1, res2);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Utility function implementations
namespace utils {

double distance(const Point3D& p1, const Point3D& p2) {
    return std::sqrt(std::pow(p1[0] - p2[0], 2) + std::pow(p1[1] - p2[1], 2) +
                     std::pow(p1[2] - p2[2], 2));
}

double distance(const Point2D& p1, const Point2D& p2) {
    return std::sqrt(std::pow(p1[0] - p2[0], 2) + std::pow(p1[1] - p2[1], 2));
}

double norm(const Direction3D& v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

double norm(const Direction2D& v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1]);
}

double dot(const Point3D& v1, const Point3D& v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

double dot(const Direction2D& v1, const Direction2D& v2) {
    return v1[0] * v2[0] + v1[1] * v2[1];
}

Point3D cross(const Point3D& v1, const Point3D& v2) {
    return {{v1[1] * v2[2] - v1[2] * v2[1], v1[2] * v2[0] - v1[0] * v2[2],
             v1[0] * v2[1] - v1[1] * v2[0]}};
}

double cross(const Direction2D& v1, const Direction2D& v2) {
    return v1[0] * v2[1] - v1[1] * v2[0];
}

double angle_between_directions(const Direction3D& v1, const Direction3D& v2) {
    // Calculate vector norms
    double norm_v1 = norm(v1);
    double norm_v2 = norm(v2);

    // Check for zero vectors
    if (norm_v1 < std::numeric_limits<double>::epsilon() ||
        norm_v2 < std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Direction vector cannot be zero");
    }

    // Calculate dot product
    double dot_product = dot(v1, v2);

    // Calculate cosine of angle, clamp to [-1, 1] to avoid numerical errors
    double cos_angle = dot_product / (norm_v1 * norm_v2);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));

    // Return angle (radians)
    return std::acos(cos_angle);
}

double angle_between_directions(const Direction2D& v1, const Direction2D& v2) {
    // Calculate vector norms
    double norm_v1 = norm(v1);
    double norm_v2 = norm(v2);

    // Check for zero vectors
    if (norm_v1 < std::numeric_limits<double>::epsilon() ||
        norm_v2 < std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Direction vector cannot be zero");
    }

    // Calculate dot product
    double dot_product = dot(v1, v2);

    // Calculate cosine of angle, clamp to [-1, 1] to avoid numerical errors
    double cos_angle = dot_product / (norm_v1 * norm_v2);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));

    // Return angle (radians)
    return std::acos(cos_angle);
}

Matrix4x4 matrix_multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result = {{{{0}}}};

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }

    return result;
}

Point4D matrix_vector_multiply(const Matrix4x4& m, const Point4D& v) {
    Point4D result = {{0.0, 0.0, 0.0, 0.0}};

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i] += m[i][j] * v[j];
        }
    }

    return result;
}

Matrix3x3 matrix_multiply(const Matrix3x3& m1, const Matrix3x3& m2) {
    Matrix3x3 result = {{{{0}}}};

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                result[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }

    return result;
}

std::array<double, 3> matrix_vector_multiply(const Matrix3x3& m,
                                             const std::array<double, 3>& v) {
    std::array<double, 3> result = {{0.0, 0.0, 0.0}};

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result[i] += m[i][j] * v[j];
        }
    }

    return result;
}

}  // namespace utils

// Line class distance_to_point member function
double Line::distance_to_point(const Point3D& point) const {
    // Calculate vector from P0 to P
    Point3D vec_p0p = {
        {point[0] - point_[0], point[1] - point_[1], point[2] - point_[2]}};

    // Cross product magnitude equals area of parallelogram
    Point3D cross_product = utils::cross(direction_, vec_p0p);

    // Distance equals area divided by base length
    return utils::norm(cross_product);
}

/**
 * Robot kinematics solver function
 * @param x, y, z: Position coordinates
 * @param roll_deg, pitch_deg, yaw_deg: Attitude angles (degrees)
 * @return Array of 6 theta angles if solution exists, otherwise std::nullopt
 */
std::optional<std::array<double, 6>> solve_robot_kinematics(double x,
                                                            double y,
                                                            double z,
                                                            double roll_deg,
                                                            double pitch_deg,
                                                            double yaw_deg,
                                                            double r,
                                                            double arm,
                                                            double link,
                                                            Point3D a,
                                                            Point3D b) {
    static int times = 0;
    bool flag = false;
    if (times++ % 50 == 0) {
        flag = true;
    }
    if (flag)
        ESP_LOGI("O6Geometry", "x: %.3f, y: %.3f, z: %.3f, roll: %.3f, pitch: %.3f, yaw: %.3f",
                 x, y, z, roll_deg, pitch_deg, yaw_deg);
    try {
        // Convert angles from degrees to radians
        auto [roll, pitch, yaw] = transform_3d::euler_degrees_to_radians(
            roll_deg, pitch_deg, yaw_deg);
        Matrix4x4 T0 = {};
        if (O6_OFFSET != 0)
            T0 = transform_3d::pose_to_homogeneous_matrix(0, 0, O6_OFFSET, roll,
                                                          pitch, yaw);
        else
            T0 = transform_3d::pose_to_homogeneous_matrix(0, 0, 0, 0, 0, 0);
        auto [x_, y_, z_] = transform_3d::transform_point(T0, Point3D{x, y, z});
        x = x_;
        y = y_;
        z = z_;
        // Calculate homogeneous transformation matrix
        Matrix4x4 T1 =
            transform_3d::pose_to_homogeneous_matrix(x, y, z, roll, pitch, yaw);

        // Three platform connection points: generate and transform all at once
        std::vector<Point3D> mapped;
        for (int i = 0; i < 3; ++i) {
            double t = i * 2 * M_PI / 3;
            Point3D local = {{r * std::cos(t), r * std::sin(t), 0}};
            mapped.push_back(transform_3d::transform_point(T1, local));
        }
        if (flag) {
            // Print mapped points
            for (const auto& pt : mapped) {
                ESP_LOGI("O6Geometry", "mapped: %.3f, %.3f, %.3f", pt[0], pt[1], pt[2]);
            }
        }
        static auto Ta0 = geometry::transform_3d::pose_to_homogeneous_matrix(
            0, 0, 0, 0, 0, 0);
        static auto Ta1 = geometry::transform_3d::pose_to_homogeneous_matrix(
            0, 0, 0, 0, 0, 2 * M_PI / 3);
        static auto Ta2 = geometry::transform_3d::pose_to_homogeneous_matrix(
            0, 0, 0, 0, 0, 2 * M_PI / 3 * 2);

        // Define a_l and b_l points
        std::vector<Point3D> a_l = {
            geometry::transform_3d::transform_point(Ta0, a),
            geometry::transform_3d::transform_point(Ta1, a),
            geometry::transform_3d::transform_point(Ta2, a)};

        std::vector<Point3D> b_l = {
            geometry::transform_3d::transform_point(Ta0, b),
            geometry::transform_3d::transform_point(Ta1, b),
            geometry::transform_3d::transform_point(Ta2, b)};

        std::array<double, 6> thetas;
        // Process each point

        for (int i = 0; i < 3; i++) {
            Point3D p = mapped[i];
            Point3D a = a_l[i];
            Point3D b = b_l[i];

            // Create line ab
            Line l_ab(a, b);

            // Calculate perpendicular line
            Line vel = l_ab.vertical_line(p);

            // Calculate intersection
            auto intersection = l_ab.intersection_with(vel);
            if (!intersection) {
                return std::nullopt;
            }

            // Create line from intersection to p
            Line l(*intersection, p);

            // Calculate angle
            Direction3D m = {{p[0] - (*intersection)[0],
                              p[1] - (*intersection)[1],
                              p[2] - (*intersection)[2]}};
            Direction3D n = {{0, 0, 1}};

            double angle = utils::angle_between_directions(m, n);

            // Rotate point
            Point3D p1 = l_ab.rotate_point_around_line(p, angle);

            if (flag) {
                ESP_LOGI("O6Geometry", "angle: %.1f",
                         angle * 180 / M_PI * (angle < 0 ? -1 : 1));
            }

            // Calculate five-bar parameters
            auto result = five_bar_back_kinematics(a, b, arm, link, p1);
            if (!result) {
                return std::nullopt;
            } else {
                auto [theta1, theta2] = *result;
                thetas[i * 2] = theta1;
                thetas[i * 2 + 1] = theta2;
            }
        }

        return thetas;

    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Point3D operator implementations
Point3D operator+(const Point3D& lhs, const Point3D& rhs) {
    return {{lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]}};
}

Point3D operator-(const Point3D& lhs, const Point3D& rhs) {
    return {{lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]}};
}

Point3D operator*(const Point3D& lhs, double scalar) {
    return {{lhs[0] * scalar, lhs[1] * scalar, lhs[2] * scalar}};
}

Point3D operator*(double scalar, const Point3D& rhs) {
    return {{scalar * rhs[0], scalar * rhs[1], scalar * rhs[2]}};
}

Point3D operator/(const Point3D& lhs, double scalar) {
    if (std::abs(scalar) < std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Divisor cannot be zero");
    }
    return {{lhs[0] / scalar, lhs[1] / scalar, lhs[2] / scalar}};
}

// Direction2D operator implementations
Direction2D operator+(const Direction2D& lhs, const Direction2D& rhs) {
    return {{lhs[0] + rhs[0], lhs[1] + rhs[1]}};
}

Direction2D operator-(const Direction2D& lhs, const Direction2D& rhs) {
    return {{lhs[0] - rhs[0], lhs[1] - rhs[1]}};
}

Direction2D operator*(const Direction2D& lhs, double scalar) {
    return {{lhs[0] * scalar, lhs[1] * scalar}};
}

Direction2D operator*(double scalar, const Direction2D& rhs) {
    return {{scalar * rhs[0], scalar * rhs[1]}};
}

Direction2D operator/(const Direction2D& lhs, double scalar) {
    if (std::abs(scalar) < std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument("Divisor cannot be zero");
    }
    return {{lhs[0] / scalar, lhs[1] / scalar}};
}

}  // namespace geometry
