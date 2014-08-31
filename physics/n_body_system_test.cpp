﻿#include "physics/n_body_system.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "geometry/grassmann.hpp"
#include "geometry/point.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "physics/body.hpp"
#include "physics/trajectory.hpp"
#include "quantities/constants.hpp"
#include "quantities/numbers.hpp"
#include "testing_utilities/almost_equals.hpp"
#include "testing_utilities/death_message.hpp"

using principia::constants::GravitationalConstant;
using principia::geometry::Barycentre;
using principia::geometry::Point;
using principia::geometry::Vector;
using principia::quantities::Pow;
using principia::quantities::SIUnit;
using principia::testing_utilities::AlmostEquals;
using principia::testing_utilities::DeathMessage;
using testing::Eq;
using testing::Lt;

namespace principia {
namespace physics {

class NBodySystemTest : public testing::Test {
 protected:
  struct EarthMoonBarycentricFrame;

  void SetUp() override {
    integrator_.Initialize(integrator_.Order5Optimal());

    // The Earth-Moon system, roughly, with a circular orbit with velocities
    // in the center-of-mass frame.
    body1_ = new Body(6E24 * SIUnit<Mass>());
    body2_ = new Body(7E22 * SIUnit<Mass>());

    // A massless probe.
    body3_ = new Body(0 * SIUnit<Mass>());

    trajectory1_.reset(new Trajectory<EarthMoonBarycentricFrame>(*body1_));
    trajectory2_.reset(new Trajectory<EarthMoonBarycentricFrame>(*body2_));
    trajectory3_.reset(new Trajectory<EarthMoonBarycentricFrame>(*body3_));
    Point<Vector<Length, EarthMoonBarycentricFrame>> const
        q1(Vector<Length, EarthMoonBarycentricFrame>({0 * SIUnit<Length>(),
                                                      0 * SIUnit<Length>(),
                                                      0 * SIUnit<Length>()}));
    Point<Vector<Length, EarthMoonBarycentricFrame>> const
        q2(Vector<Length, EarthMoonBarycentricFrame>({0 * SIUnit<Length>(),
                                                      4E8 * SIUnit<Length>(),
                                                      0 * SIUnit<Length>()}));
    Point<Vector<Length, EarthMoonBarycentricFrame>> const centre_of_mass =
        Barycentre(q1, body1_->mass(), q2, body2_->mass());
    Length const semi_major_axis = (q1 - q2).Norm();
    period_ = 2 * π * Sqrt(Pow<3>(semi_major_axis) /
                               (body1_->gravitational_parameter() +
                                body2_->gravitational_parameter()));
    Point<Vector<Speed, EarthMoonBarycentricFrame>> const
        v1(Vector<Speed, EarthMoonBarycentricFrame>({
            -2 * π * (q1 - centre_of_mass).Norm() / period_,
            0 * SIUnit<Speed>(),
            0 * SIUnit<Speed>()}));
    Point<Vector<Speed, EarthMoonBarycentricFrame>> const
        v2(Vector<Speed, EarthMoonBarycentricFrame>({
            2 * π * (q2 - centre_of_mass).Norm() / period_,
            0 * SIUnit<Speed>(),
            0 * SIUnit<Speed>()}));
    Point<Vector<Speed, EarthMoonBarycentricFrame>> const overall_velocity =
        Barycentre(v1, body1_->mass(), v2, body2_->mass());
    trajectory1_->Append(0 * SIUnit<Time>(),
                         {q1 - centre_of_mass, v1 - overall_velocity});
    trajectory2_->Append(0 * SIUnit<Time>(),
                         {q2 - centre_of_mass, v2 - overall_velocity});
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massive_bodies;
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massless_bodies;
    massive_bodies.emplace_back(body1_);
    massive_bodies.emplace_back(body2_);
    massless_bodies.emplace_back(body3_);
    system_ = std::make_unique<NBodySystem<EarthMoonBarycentricFrame>>(
                  std::move(massive_bodies),
                  std::move(massless_bodies));
  }

  template<typename Scalar, typename Frame>
  std::string ToMathematicaString(Vector<Scalar, Frame> const& vector) {
    R3Element<Scalar> const& coordinates = vector.coordinates();
    std::string result = "{";
    result += quantities::DebugString(coordinates.x);
    result += ",";
    result += quantities::DebugString(coordinates.y);
    result += ",";
    result += quantities::DebugString(coordinates.z);
    result += "}";
    return result;
  }

  template<typename Scalar, typename Frame>
  std::string ToMathematicaString(
      std::vector<Vector<Scalar, Frame>> const& vectors) {
    static std::string const mathematica_line =
        "\n(*****************************************************)\n";
    std::string result = mathematica_line;
    result += "ToExpression[StringReplace[\"\n{";
    std::string separator = "";
    for (const auto& vector : vectors) {
      result += separator;
      result += ToMathematicaString(vector);
      separator = ",\n";
    }
    result +=
        "}\",\n{\" m\"->\"\",\"e\"->\"*^\", \"\\n\"->\"\", \" \"->\"\"}]];";
    result += mathematica_line;
    return result;
  }

  template<typename T1, typename T2>
  std::vector<T2> ValuesOf(std::map<T1, T2> const& m) {
    std::vector<T2> result;
    for (auto const it : m) {
      result.push_back(it.second);
    }
    return result;
  }

  Body* body1_;
  Body* body2_;
  Body* body3_;
  std::unique_ptr<Trajectory<EarthMoonBarycentricFrame>> trajectory1_;
  std::unique_ptr<Trajectory<EarthMoonBarycentricFrame>> trajectory2_;
  std::unique_ptr<Trajectory<EarthMoonBarycentricFrame>> trajectory3_;
  SPRKIntegrator<Length, Speed> integrator_;
  Time period_;
  std::unique_ptr<NBodySystem<EarthMoonBarycentricFrame>> system_;
};

typedef NBodySystemTest NBodySystemDeathTest;

TEST_F(NBodySystemDeathTest, ConstructionError) {
  EXPECT_DEATH({
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massive_bodies;
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massless_bodies;
    massless_bodies.emplace_back(body1_);
    system_ = std::make_unique<NBodySystem<EarthMoonBarycentricFrame>>(
                  std::move(massive_bodies),
                  std::move(massless_bodies));
  }, DeathMessage("is massive"));
  EXPECT_DEATH({
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massive_bodies;
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massless_bodies;
    massive_bodies.emplace_back(body3_);
    system_ = std::make_unique<NBodySystem<EarthMoonBarycentricFrame>>(
                  std::move(massive_bodies),
                  std::move(massless_bodies));
  }, DeathMessage("is massless"));
  EXPECT_DEATH({
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massive_bodies;
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massless_bodies;
    massive_bodies.emplace_back(body1_);
    massive_bodies.emplace_back(body2_);
    massive_bodies.emplace_back(body1_);
    system_ = std::make_unique<NBodySystem<EarthMoonBarycentricFrame>>(
                  std::move(massive_bodies),
                  std::move(massless_bodies));
  }, DeathMessage("Massive.* multiple times"));
  EXPECT_DEATH({
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massive_bodies;
    NBodySystem<EarthMoonBarycentricFrame>::Bodies massless_bodies;
    massless_bodies.emplace_back(body3_);
    massless_bodies.emplace_back(body3_);
    system_ = std::make_unique<NBodySystem<EarthMoonBarycentricFrame>>(
                  std::move(massive_bodies),
                  std::move(massless_bodies));
  }, DeathMessage("Massless.* multiple times"));
}

TEST_F(NBodySystemDeathTest, IntegrateError) {
  EXPECT_DEATH({
    system_->Integrate(integrator_,
                       period_,
                       period_ / 100,
                       1,
                       {trajectory1_.get(),
                        trajectory2_.get(),
                        trajectory1_.get()});
  }, DeathMessage("Multiple trajectories"));
  EXPECT_DEATH({
    std::unique_ptr<Body> body(new Body(0 * SIUnit<Mass>()));
    std::unique_ptr<Trajectory<EarthMoonBarycentricFrame>> trajectory(
        new Trajectory<EarthMoonBarycentricFrame>(*body));
    trajectory->Append(0 * SIUnit<Time>(),
                       {Vector<Length, EarthMoonBarycentricFrame>(),
                        Vector<Speed, EarthMoonBarycentricFrame>()});
    system_->Integrate(integrator_,
                       period_,
                       period_ / 100,
                       1,
                       {trajectory1_.get(), trajectory.get()});
  }, DeathMessage("unknown body"));
  EXPECT_DEATH({
    std::unique_ptr<Trajectory<EarthMoonBarycentricFrame>> trajectory(
        new Trajectory<EarthMoonBarycentricFrame>(*body2_));
    trajectory->Append(1 * SIUnit<Time>(),
                       {Vector<Length, EarthMoonBarycentricFrame>(),
                        Vector<Speed, EarthMoonBarycentricFrame>()});
    system_->Integrate(integrator_,
                       period_,
                       period_ / 100,
                       1,
                       {trajectory1_.get(), trajectory.get()});
  }, DeathMessage("Inconsistent last time"));
}

// The canonical Earth-Moon system, tuned to produce circular orbits.
TEST_F(NBodySystemTest, EarthMoon) {
  std::vector<Vector<Length, EarthMoonBarycentricFrame>> positions;
  system_->Integrate(integrator_,
                     period_,
                     period_ / 100,
                     1,
                     {trajectory1_.get(), trajectory2_.get()});

  positions = ValuesOf(trajectory1_->Positions());
  EXPECT_THAT(positions.size(), Eq(101));
  LOG(INFO) << ToMathematicaString(positions);
  EXPECT_THAT(Abs(positions[25].coordinates().y), Lt(3E-2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[50].coordinates().x), Lt(3E-2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[75].coordinates().y), Lt(3E-2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[100].coordinates().x), Lt(3E-2 * SIUnit<Length>()));

  positions = ValuesOf(trajectory2_->Positions());
  LOG(INFO) << ToMathematicaString(positions);
  EXPECT_THAT(positions.size(), Eq(101));
  EXPECT_THAT(Abs(positions[25].coordinates().y), Lt(2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[50].coordinates().x), Lt(2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[75].coordinates().y), Lt(2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[100].coordinates().x), Lt(2 * SIUnit<Length>()));
}

// Same as above, but the trajectories are passed in the reverse order.
TEST_F(NBodySystemTest, MoonEarth) {
  std::vector<Vector<Length, EarthMoonBarycentricFrame>> positions;
  system_->Integrate(integrator_,
                     period_,
                     period_ / 100,
                     1,
                     {trajectory2_.get(), trajectory1_.get()});

  positions = ValuesOf(trajectory1_->Positions());
  EXPECT_THAT(positions.size(), Eq(101));
  LOG(INFO) << ToMathematicaString(positions);
  EXPECT_THAT(Abs(positions[25].coordinates().y), Lt(3E-2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[50].coordinates().x), Lt(3E-2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[75].coordinates().y), Lt(3E-2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[100].coordinates().x), Lt(3E-2 * SIUnit<Length>()));

  positions = ValuesOf(trajectory2_->Positions());
  LOG(INFO) << ToMathematicaString(positions);
  EXPECT_THAT(positions.size(), Eq(101));
  EXPECT_THAT(Abs(positions[25].coordinates().y), Lt(2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[50].coordinates().x), Lt(2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[75].coordinates().y), Lt(2 * SIUnit<Length>()));
  EXPECT_THAT(Abs(positions[100].coordinates().x), Lt(2 * SIUnit<Length>()));
}

// The Moon alone.  It moves in straight line.
TEST_F(NBodySystemTest, Moon) {
  system_->Integrate(integrator_,
                     period_,
                     period_ / 100,
                     1,
                     {trajectory2_.get()});

  Length const q2 = trajectory2_->last_position().coordinates().y;
  Speed const v2 = trajectory2_->last_velocity().coordinates().x;
  std::vector<Vector<Length, EarthMoonBarycentricFrame>> const positions =
      ValuesOf(trajectory2_->Positions());
  LOG(INFO) << ToMathematicaString(positions);
  EXPECT_THAT(positions.size(), Eq(101));
  EXPECT_THAT(positions[25].coordinates().x, Eq(0.25 * period_ * v2));
  EXPECT_THAT(positions[25].coordinates().y, Eq(q2));
  EXPECT_THAT(positions[50].coordinates().x, Eq(0.50 * period_ * v2));
  EXPECT_THAT(positions[50].coordinates().y, Eq(q2));
  EXPECT_THAT(positions[75].coordinates().x,
              AlmostEquals(0.75 * period_ * v2, 1));
  EXPECT_THAT(positions[75].coordinates().y, Eq(q2));
  EXPECT_THAT(positions[100].coordinates().x, Eq(1.00 * period_ * v2));
  EXPECT_THAT(positions[100].coordinates().y, Eq(q2));
}

// TODO(phl): Test the error cases.

// The Earth and a massless probe 1 billion meters away, with the same velocity,
// and an acceleration which exactly compensates gravitational attraction.  Both
// bodies move in straight lines.
TEST_F(NBodySystemTest, EarthProbe) {
  Length const distance = 1E9 * SIUnit<Length>();
  trajectory3_->Append(trajectory1_->last_time(),
                       {trajectory1_->last_position() +
                            Vector<Length, EarthMoonBarycentricFrame>(
                                {0 * SIUnit<Length>(),
                                 distance,
                                 0 * SIUnit<Length>()}),
                        trajectory1_->last_velocity()});
  trajectory3_->set_intrinsic_acceleration(
      [this, distance](Time const& t) {
    return Vector<Acceleration, EarthMoonBarycentricFrame>(
        {0 * SIUnit<Acceleration>(),
         body1_->gravitational_parameter() / (distance * distance),
         0 * SIUnit<Acceleration>()});});

  system_->Integrate(integrator_,
                     period_,
                     period_ / 100,
                     1,
                     {trajectory1_.get(), trajectory3_.get()});

  Length const q1 = trajectory1_->last_position().coordinates().y;
  Speed const v1 = trajectory1_->last_velocity().coordinates().x;
  std::vector<Vector<Length, EarthMoonBarycentricFrame>> const positions1 =
      ValuesOf(trajectory1_->Positions());
  LOG(INFO) << ToMathematicaString(positions1);
  EXPECT_THAT(positions1.size(), Eq(101));
  EXPECT_THAT(positions1[25].coordinates().x,
              AlmostEquals(0.25 * period_ * v1, 1));
  EXPECT_THAT(positions1[25].coordinates().y, Eq(q1));
  EXPECT_THAT(positions1[50].coordinates().x,
              AlmostEquals(0.50 * period_ * v1, 1));
  EXPECT_THAT(positions1[50].coordinates().y, Eq(q1));
  EXPECT_THAT(positions1[75].coordinates().x,
              AlmostEquals(0.75 * period_ * v1, 1));
  EXPECT_THAT(positions1[75].coordinates().y, Eq(q1));
  EXPECT_THAT(positions1[100].coordinates().x,
              AlmostEquals(1.00 * period_ * v1, 1));
  EXPECT_THAT(positions1[100].coordinates().y, Eq(q1));

  Length const q3 = trajectory3_->last_position().coordinates().y;
  Speed const v3 = trajectory3_->last_velocity().coordinates().x;
  std::vector<Vector<Length, EarthMoonBarycentricFrame>> const positions3 =
      ValuesOf(trajectory3_->Positions());
  LOG(INFO) << ToMathematicaString(positions3);
  EXPECT_THAT(positions3.size(), Eq(101));
  EXPECT_THAT(positions3[25].coordinates().x,
              AlmostEquals(0.25 * period_ * v3, 1));
  EXPECT_THAT(positions3[25].coordinates().y, AlmostEquals(q3, 2));
  EXPECT_THAT(positions3[50].coordinates().x,
              AlmostEquals(0.50 * period_ * v3, 1));
  EXPECT_THAT(positions3[50].coordinates().y, AlmostEquals(q3, 2));
  EXPECT_THAT(positions3[75].coordinates().x,
              AlmostEquals(0.75 * period_ * v3, 1));
  EXPECT_THAT(positions3[75].coordinates().y, AlmostEquals(q3, 1));
  EXPECT_THAT(positions3[100].coordinates().x,
              AlmostEquals(1.00 * period_ * v3, 1));
  EXPECT_THAT(positions3[100].coordinates().y, Eq(q3));
}


}  // namespace physics
}  // namespace principia
