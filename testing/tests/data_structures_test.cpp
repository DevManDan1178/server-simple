#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "data_structures/global_boards/leaderboard.hpp"
#include "data_structures/global_boards/nameboard.hpp"
#include "data_structures/global_boards/score_stream.hpp"

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    path = std::filesystem::temp_directory_path() / (
      "server_simple_tests_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()
      )
    );
    std::filesystem::create_directories(path);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  const std::filesystem::path& get() const { return path; }

private:
  std::filesystem::path path;
};

}  // namespace

TEST(Leaderboard, RanksHighestScoreFirst) {
  TemporaryDirectory directory;

  leaderboard<int> board(directory.get() / "leaderboard.json");

  auto [alice_position, _] = board.submit_score("alice", 100);
  auto [bob_position, __] = board.submit_score("bob", 200);

  ASSERT_TRUE(alice_position.has_value());
  ASSERT_TRUE(bob_position.has_value());

  const auto top = board.get_top_scores(2);

  ASSERT_EQ(top.size(), 2);

  EXPECT_EQ(top[0].name, "bob");
  EXPECT_EQ(top[0].score, 200);

  EXPECT_EQ(top[1].name, "alice");
  EXPECT_EQ(top[1].score, 100);
}

TEST(Leaderboard, WorseScoreDoesNotReplaceExistingScore) {
  TemporaryDirectory directory;

  leaderboard<int> board(directory.get() / "leaderboard.json");

  ASSERT_TRUE(std::get<0>(board.submit_score("alice", 100)).has_value());

  const auto result = board.submit_score("alice", 50);

  EXPECT_FALSE(std::get<0>(result).has_value());

  const auto top = board.get_top_scores(1);

  ASSERT_EQ(top.size(), 1);
  EXPECT_EQ(top[0].score, 100);
}

TEST(Leaderboard, CapacityRejectsScoresBelowWorst) {
  TemporaryDirectory directory;

  leaderboard<int> board(directory.get() / "leaderboard.json", 2);

  EXPECT_TRUE(std::get<0>(board.submit_score("alice", 100)).has_value());
  EXPECT_TRUE(std::get<0>(board.submit_score("bob", 200)).has_value());
  EXPECT_FALSE(std::get<0>(board.submit_score("charlie", 50)).has_value());

  EXPECT_EQ(board.size(), 2);
}

TEST(Leaderboard, SavesAndReloads) {
  TemporaryDirectory directory;

  const auto file = directory.get() / "leaderboard.json";

  {
    leaderboard<int> board(file);

    board.submit_score("alice", 100);
    board.submit_score("bob", 200);

    ASSERT_TRUE(board.save());
  }

  {
    leaderboard<int> board(file);

    const auto top = board.get_top_scores(2);

    ASSERT_EQ(top.size(), 2);
    EXPECT_EQ(top[0].name, "bob");
    EXPECT_EQ(top[0].score, 200);
    EXPECT_EQ(top[1].name, "alice");
    EXPECT_EQ(top[1].score, 100);
  }
}

TEST(Nameboard, AddsAndRanksNames) {
  TemporaryDirectory directory;

  nameboard board(directory.get() / "names.json");

  const auto first = board.add_name("alice");
  const auto second = board.add_name("bob");

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_TRUE(board.contains("alice"));
  EXPECT_TRUE(board.contains("bob"));

  EXPECT_EQ(board.size(), 2);
}

TEST(Nameboard, EnforcesMaximumSize) {
  TemporaryDirectory directory;

  nameboard board(directory.get() / "names.json", 2);

  board.add_name("alice");
  board.add_name("bob");
  board.add_name("charlie");

  EXPECT_EQ(board.size(), 2);
}

TEST(Nameboard, SavesAndReloads) {
  TemporaryDirectory directory;

  const auto file = directory.get() / "names.json";

  {
    nameboard board(file);

    board.add_name("alice");
    board.add_name("bob");

    ASSERT_TRUE(board.save());
  }

  {
    nameboard board(file);

    EXPECT_TRUE(board.contains("alice"));
    EXPECT_TRUE(board.contains("bob"));
    EXPECT_EQ(board.size(), 2);
  }
}

TEST(ScoreStream, NewScoresAppearAtFront) {
  TemporaryDirectory directory;

  score_stream<int> stream(directory.get() / "scores.json");

  stream.submit_score("alice", 100);
  stream.submit_score("bob", 200);

  const auto entries = stream.get_all();

  ASSERT_EQ(entries.size(), 2);

  EXPECT_EQ(entries[0].name, "bob");
  EXPECT_EQ(entries[0].score, 200);

  EXPECT_EQ(entries[1].name, "alice");
  EXPECT_EQ(entries[1].score, 100);
}

TEST(ScoreStream, EnforcesMaximumSize) {
  TemporaryDirectory directory;

  score_stream<int> stream(directory.get() / "scores.json", 2);

  stream.submit_score("alice", 100);
  stream.submit_score("bob", 200);
  stream.submit_score("charlie", 300);

  EXPECT_EQ(stream.size(), 2);

  const auto entries = stream.get_all();

  ASSERT_EQ(entries.size(), 2);

  EXPECT_EQ(entries[0].score, 300);
  EXPECT_EQ(entries[1].score, 200);
}

TEST(ScoreStream, SavesAndReloads) {
  TemporaryDirectory directory;

  const auto file = directory.get() / "scores.json";

  {
    score_stream<int> stream(file);

    stream.submit_score("alice", 100);
    stream.submit_score("bob", 200);

    ASSERT_TRUE(stream.save());
  }

  {
    score_stream<int> stream(file);

    const auto entries = stream.get_all();

    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].score, 200);
    EXPECT_EQ(entries[1].score, 100);
  }
}