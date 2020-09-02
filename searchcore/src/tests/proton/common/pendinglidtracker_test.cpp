// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/testkit/testapp.h>
#include <vespa/searchcore/proton/common/pendinglidtracker.h>

#include <vespa/log/log.h>
LOG_SETUP("pendinglidtracker_test");

using namespace proton;

constexpr uint32_t LID_1 = 1u;
const std::vector<uint32_t> LIDV_2_1_3({2u, LID_1, 3u});
const std::vector<uint32_t> LIDV_2_3({2u, 3u});

namespace proton {

std::ostream &
operator << (std::ostream & os, ILidCommitState::State state) {
    switch (state) {
    case ILidCommitState::State::NEED_COMMIT:
        os << "NEED_COMMIT";
        break;
    case ILidCommitState::State::WAITING:
        os << "WAITING";
        break;
    case ILidCommitState::State::COMPLETED:
        os << "COMPLETED";
        break;
    }
    return os;
}

}

void
verifyPhase1ProduceAndNeedCommit(PendingLidTrackerBase & tracker, ILidCommitState::State expected) {
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState());
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LID_1));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_1_3));

    auto token = tracker.produce(LID_1);
    EXPECT_EQUAL(expected, tracker.getState());
    EXPECT_EQUAL(expected, tracker.getState(LID_1));
    EXPECT_EQUAL(expected, tracker.getState(LIDV_2_1_3));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
    {
        auto token2 = tracker.produce(LID_1);
        EXPECT_EQUAL(expected, tracker.getState());
        EXPECT_EQUAL(expected, tracker.getState(LID_1));
        EXPECT_EQUAL(expected, tracker.getState(LIDV_2_1_3));
        EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
    }
    EXPECT_EQUAL(expected, tracker.getState());
    EXPECT_EQUAL(expected, tracker.getState(LID_1));
    EXPECT_EQUAL(expected, tracker.getState(LIDV_2_1_3));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
}

TEST("test pendinglidtracker for needcommit") {
    PendingLidTracker tracker;
    verifyPhase1ProduceAndNeedCommit(tracker, ILidCommitState::State::WAITING);
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState());
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LID_1));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_1_3));
    {
        ILidCommitState::State incomplete = ILidCommitState::State::WAITING;
        auto token = tracker.produce(LID_1);
        EXPECT_EQUAL(incomplete, tracker.getState());
        EXPECT_EQUAL(incomplete, tracker.getState(LID_1));
        EXPECT_EQUAL(incomplete, tracker.getState(LIDV_2_1_3));
        EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
        {
            auto snapshot = tracker.produceSnapshot();
            EXPECT_EQUAL(incomplete, tracker.getState());
            EXPECT_EQUAL(incomplete, tracker.getState(LID_1));
            EXPECT_EQUAL(incomplete, tracker.getState(LIDV_2_1_3));
            EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
        }
        EXPECT_EQUAL(incomplete, tracker.getState());
        EXPECT_EQUAL(incomplete, tracker.getState(LID_1));
        EXPECT_EQUAL(incomplete, tracker.getState(LIDV_2_1_3));
        EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
    }
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState());
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LID_1));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_1_3));
}

TEST("test two phase pendinglidtracker for needcommit") {
    TwoPhasePendingLidTracker tracker;
    ILidCommitState::State incomplete = ILidCommitState::State::NEED_COMMIT;
    verifyPhase1ProduceAndNeedCommit(tracker, incomplete);
    EXPECT_EQUAL(incomplete, tracker.getState());
    EXPECT_EQUAL(incomplete, tracker.getState(LID_1));
    EXPECT_EQUAL(incomplete, tracker.getState(LIDV_2_1_3));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_3));
    {
        ILidCommitState::State waiting = ILidCommitState::State::WAITING;
        auto snapshot = tracker.produceSnapshot();
        EXPECT_EQUAL(waiting, tracker.getState());
        EXPECT_EQUAL(waiting, tracker.getState(LID_1));
        EXPECT_EQUAL(waiting, tracker.getState(LIDV_2_1_3));
    }
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState());
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LID_1));
    EXPECT_EQUAL(ILidCommitState::State::COMPLETED, tracker.getState(LIDV_2_1_3));
}

TEST_MAIN() { TEST_RUN_ALL(); }